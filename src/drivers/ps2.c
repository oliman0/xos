#include "kernel/drivers/acpi.h"
#include <kernel/drivers/ps2.h>
#include <kernel/drivers/ioapic.h>
#include <kernel/arch/io.h>
#include <kernel/idt.h>
#include <kernel/kernel_io.h>
#include <kernel/drivers/lapic.h>
#include <stdbool.h>
#include <kernel/lib/ring_buffer.h>
#include <kernel/input.h>
#include <stdint.h>
#include <sys/types.h>

static spsc_ring_buffer_t* ring_buffer;
static uint8_t modifiers = 0;

static const key_code_t ps2_set1_map[128] = {
    [0x00] = KEY_NONE,
    [0x01] = KEY_ESCAPE,
    [0x02] = KEY_1,         [0x03] = KEY_2,         [0x04] = KEY_3,         [0x05] = KEY_4,
    [0x06] = KEY_5,         [0x07] = KEY_6,         [0x08] = KEY_7,         [0x09] = KEY_8,
    [0x0A] = KEY_9,         [0x0B] = KEY_0,         [0x0C] = KEY_MINUS,     [0x0D] = KEY_EQUAL,
    [0x0E] = KEY_BACKSPACE, [0x0F] = KEY_TAB,
    [0x10] = KEY_Q,         [0x11] = KEY_W,         [0x12] = KEY_E,         [0x13] = KEY_R,
    [0x14] = KEY_T,         [0x15] = KEY_Y,         [0x16] = KEY_U,         [0x17] = KEY_I,
    [0x18] = KEY_O,         [0x19] = KEY_P,         [0x1A] = KEY_LEFTBRACKET,[0x1B] = KEY_RIGHTBRACKET,
    [0x1C] = KEY_ENTER,     [0x1D] = KEY_LCTRL,
    [0x1E] = KEY_A,         [0x1F] = KEY_S,         [0x20] = KEY_D,         [0x21] = KEY_F,
    [0x22] = KEY_G,         [0x23] = KEY_H,         [0x24] = KEY_J,         [0x25] = KEY_K,
    [0x26] = KEY_L,         [0x27] = KEY_SEMICOLON, [0x28] = KEY_APOSTROPHE,[0x29] = KEY_GRAVE,
    [0x2A] = KEY_LSHIFT,    [0x2B] = KEY_BACKSLASH,
    [0x2C] = KEY_Z,         [0x2D] = KEY_X,         [0x2E] = KEY_C,         [0x2F] = KEY_V,
    [0x30] = KEY_B,         [0x31] = KEY_N,         [0x32] = KEY_M,         [0x33] = KEY_COMMA,
    [0x34] = KEY_DOT,       [0x35] = KEY_SLASH,     [0x36] = KEY_RSHIFT,    [0x37] = KEY_KP_MULTIPLY,
    [0x38] = KEY_LALT,      [0x39] = KEY_SPACE,     [0x3A] = KEY_CAPSLOCK,
    [0x3B] = KEY_F1,        [0x3C] = KEY_F2,        [0x3D] = KEY_F3,        [0x3E] = KEY_F4,
    [0x3F] = KEY_F5,        [0x40] = KEY_F6,        [0x41] = KEY_F7,        [0x42] = KEY_F8,
    [0x43] = KEY_F9,        [0x44] = KEY_F10,
    [0x45] = KEY_NUMLOCK,   [0x46] = KEY_SCROLLLOCK,
    [0x47] = KEY_KP_7,      [0x48] = KEY_KP_8,      [0x49] = KEY_KP_9,      [0x4A] = KEY_KP_MINUS,
    [0x4B] = KEY_KP_4,      [0x4C] = KEY_KP_5,      [0x4D] = KEY_KP_6,      [0x4E] = KEY_KP_PLUS,
    [0x4F] = KEY_KP_1,      [0x50] = KEY_KP_2,      [0x51] = KEY_KP_3,      [0x52] = KEY_KP_0,
    [0x53] = KEY_KP_DECIMAL,
    [0x57] = KEY_F11,       [0x58] = KEY_F12,
};

// Extended PS/2 Set 1 mapping table (Evaluated when byte is preceded by 0xE0)
static const key_code_t ps2_set1_ext_map[128] = {
    [0x1C] = KEY_KP_ENTER,
    [0x1D] = KEY_RCTRL,
    [0x35] = KEY_KP_DIVIDE,
    [0x38] = KEY_RALT,
    [0x47] = KEY_HOME,      [0x48] = KEY_UP,        [0x49] = KEY_PAGEUP,
    [0x4B] = KEY_LEFT,      [0x4D] = KEY_RIGHT,
    [0x4F] = KEY_END,       [0x50] = KEY_DOWN,      [0x51] = KEY_PAGEDOWN,
    [0x52] = KEY_INSERT,    [0x53] = KEY_DELETE,
    [0x5B] = KEY_LMETA,     [0x5C] = KEY_RMETA,
};

static bool ps2_wait_read() {
    uint64_t timeout = lapic_get_kernel_ticks() + PS2_IO_TIMEOUT;
    while (!(inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL) && lapic_get_kernel_ticks() < timeout);
    return lapic_get_kernel_ticks() < timeout;
}

static bool ps2_wait_write() {
    uint64_t timeout = lapic_get_kernel_ticks() + PS2_IO_TIMEOUT;
    while ((inb(PS2_STATUS_PORT) & PS2_STATUS_INPUT_FULL) && lapic_get_kernel_ticks() < timeout);
    return lapic_get_kernel_ticks() < timeout;
}

static bool extended = false;

static bool handle_keyboard_scancode(uint8_t scancode, key_code_t* out_key_code, bool* out_pressed) {
    *out_pressed = !(scancode & PS2_KBD_RELEASE);

    if (scancode == PS2_SCANCODE_EXTENDED)
    {
        extended = true;
        return false;
    }

    if (extended)
    {
        *out_key_code = ps2_set1_ext_map[scancode & ~PS2_KBD_RELEASE];
        extended = false;
        return true;
    }

    *out_key_code = ps2_set1_map[scancode & ~PS2_KBD_RELEASE];

    return true;
}

static void update_modifier_bit(uint8_t bitmask, bool active) {
    if (active) modifiers |= bitmask;
    else modifiers &= ~bitmask;
}

static void process_modifers(keyboard_event_t* event)
{
    if (event->pressed)
    {
        if (event->key_code == KEY_CAPSLOCK) modifiers ^= MOD_CAPSLOCK;
        if (event->key_code == KEY_NUMLOCK)  modifiers ^= MOD_NUMLOCK;
    }

    switch (event->key_code)
    {
        case KEY_LSHIFT: update_modifier_bit(MOD_LSHIFT, event->pressed); break;
        case KEY_RSHIFT: update_modifier_bit(MOD_RSHIFT, event->pressed); break;
        case KEY_LCTRL:  update_modifier_bit(MOD_LCTRL,  event->pressed); break;
        case KEY_RCTRL:  update_modifier_bit(MOD_RCTRL,  event->pressed); break;
        case KEY_LALT:   update_modifier_bit(MOD_LALT,   event->pressed); break;
        case KEY_RALT:   update_modifier_bit(MOD_RALT,   event->pressed); break;
        default: break;
    }
}

static void keyboard_irq_handler(registers_t* regs) {
    while (1)
    {
        uint8_t status = inb(PS2_STATUS_PORT);

        if (!(status & PS2_STATUS_OUTPUT_FULL)) break;

        if (status & PS2_STATUS_AUX_DATA) break;

        uint8_t scancode = inb(PS2_DATA_PORT);
        spsc_ring_buffer_push(ring_buffer, &scancode);
    }

    lapic_eoi();
}

static void mouse_irq_handler(registers_t* regs) {
    while (1)
    {
        uint8_t status = inb(PS2_STATUS_PORT);

        if (!(status & PS2_STATUS_OUTPUT_FULL)) break;

        if (!(status & PS2_STATUS_AUX_DATA)) break;

        uint8_t data = inb(PS2_DATA_PORT);
    }

    lapic_eoi();
}

bool ps2_init() 
{
    kprintf("Initializing PS/2 Driver...\n");

    ring_buffer = spsc_ring_buffer_init(256, sizeof(uint8_t));
    if (!ring_buffer) return false;

    // Check if the i8042 controller exsists
    bool i8042_present = true;

    acpi_fadt_t* fadt = (acpi_fadt_t*)acpi_find_table(ACPI_SIGNATURE_FADT);
    if (fadt == NULL) return false;

    // Check bit 1 (value 0x02) of the iapc boot arch flags (i8042 flag)
    if (fadt->header.revision >= 2)
    {
        i8042_present = (fadt->boot_architecture_flags & ACPI_FADT_IAPC_8042_FLAG) != 0;
    }

    if (!i8042_present)
    {
        kprintf("   8042 controller not present\n");
        return false;
    }

    // Disable devices
    if (!ps2_wait_write()) return false;
    outb(PS2_COMMAND_PORT, PS2_CMD_DISABLE_PORT1);
    if (!ps2_wait_write()) return false;
    outb(PS2_COMMAND_PORT, PS2_CMD_DISABLE_PORT2);

    // Flush output buffer
    while (inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL) {
        inb(PS2_DATA_PORT);
    }

    // Perform self test
    if (!ps2_wait_write()) return false;
    outb(PS2_COMMAND_PORT, PS2_CMD_SELF_TEST);
    if (!ps2_wait_read()) return false;
    uint8_t test_response = inb(PS2_DATA_PORT);

    if (test_response != PS2_SELF_TEST_SUCCESS)
    {
        kprintf("   Controller Self Test Fail: %x ", test_response);
        return false;
    }

    // Read config byte
    if (!ps2_wait_write()) return false;
    outb(PS2_COMMAND_PORT, PS2_CMD_READ_CONFIG);
    if (!ps2_wait_read()) return false;
    uint8_t config = inb(PS2_DATA_PORT);

    // Configure port 1 and write config byte
    config |= PS2_CONFIG_TRANSLATION;
    config &= ~PS2_CONFIG_PORT1_INT;
    config &= ~PS2_CONFIG_PORT1_CLK;

    if (!ps2_wait_write()) return false;
    outb(PS2_COMMAND_PORT, PS2_CMD_WRITE_CONFIG);
    if (!ps2_wait_write()) return false;
    outb(PS2_DATA_PORT, config);

    // Test dual channel
    bool dual_channel = false;

    if (!ps2_wait_write()) return false;
    outb(PS2_COMMAND_PORT, PS2_CMD_ENABLE_PORT2);
    if (!ps2_wait_write()) return false;
    outb(PS2_COMMAND_PORT, PS2_CMD_READ_CONFIG);
    if (!ps2_wait_read()) return false;
    config = inb(PS2_DATA_PORT);

    dual_channel = !(config & PS2_CONFIG_PORT2_CLK);

    // If dual channel disable port 2 then write config byte
    if (dual_channel)
    {
        kprintf("   Dual Channel\n");

        if (!ps2_wait_write()) return false;
        outb(PS2_COMMAND_PORT, PS2_CMD_DISABLE_PORT2);

        config &= ~PS2_CONFIG_PORT2_INT;
        config &= ~PS2_CONFIG_PORT2_CLK;

        if (!ps2_wait_write()) return false;
        outb(PS2_COMMAND_PORT, PS2_CMD_WRITE_CONFIG);
        if (!ps2_wait_write()) return false;
        outb(PS2_DATA_PORT, config);
    }

    // Test PS/2 port 1
    if (!ps2_wait_write()) return false;
    outb(PS2_COMMAND_PORT, PS2_CMD_TEST_PORT1);
    if (!ps2_wait_read()) return false;
    test_response = inb(PS2_DATA_PORT);

    if (test_response != PS2_PORT_TEST_SUCCESS)
    {
        kprintf("   Port 1 Test Fail: %x\n", test_response);
        return false;
    }

    // Test PS/2 port 2
    if (dual_channel)
    {
        if (!ps2_wait_write()) return false;
        outb(PS2_COMMAND_PORT, PS2_CMD_TEST_PORT2);
        if (!ps2_wait_read()) return false;
        test_response = inb(PS2_DATA_PORT);

        if (test_response != PS2_PORT_TEST_SUCCESS)
        {
            kprintf("   Port 2 Test Fail: %x\n", test_response);
            return false;
        }
    }

    // Enable Devices
    if (!ps2_wait_write()) return false;
    outb(PS2_COMMAND_PORT, PS2_CMD_ENABLE_PORT1);
    if (dual_channel) {
        if (!ps2_wait_write()) return false;
        outb(PS2_COMMAND_PORT, PS2_CMD_ENABLE_PORT2);
    }

    // Reset Devices
    if (!ps2_wait_write()) return false;
    outb(PS2_DATA_PORT, PS2_DEV_RESET);
    if (!ps2_wait_read()) return false;
    uint8_t ack = inb(PS2_DATA_PORT);

    if (ack != PS2_ACK)
    {
        kprintf("   Port 1 Expected ACK: %x\n", ack);
        return false;
    }

    if (!ps2_wait_read()) return false;
    test_response = inb(PS2_DATA_PORT);

    if (test_response != PS2_DEV_BAT_SUCCESS)
    {
        kprintf("   Port 1 BAT Fail: %x\n", test_response);
        return false;
    }

    if (dual_channel)
    {
        if (!ps2_wait_write()) return false;
        outb(PS2_COMMAND_PORT, PS2_CMD_SEND_PORT2);
        if (!ps2_wait_write()) return false;
        outb(PS2_DATA_PORT, PS2_DEV_RESET);

        if (!ps2_wait_read()) return false;
        ack = inb(PS2_DATA_PORT);

        if (ack == PS2_ACK)
        {
            if (!ps2_wait_read()) return false;
            test_response = inb(PS2_DATA_PORT);

            if (test_response != PS2_DEV_BAT_SUCCESS)
            {
                kprintf("   Port 2 BAT Fail: %x\n", test_response);
                dual_channel = false;
            }
        }
        else
        {
            kprintf("   Port 2 Expected ACK: %x\n", ack);
            dual_channel = false;
        }

        if (!dual_channel)
        {
            kprintf("   Disabling Dual Channel");
            if (!ps2_wait_write()) return false;
            outb(PS2_COMMAND_PORT, PS2_CMD_DISABLE_PORT2);
        }
    }

    // Enable keyboard scanning
    if (!ps2_wait_write()) return false;
    outb(PS2_DATA_PORT, PS2_ENABLE_KEYBOARD_SCANNING);
    if (ps2_wait_read()) {
        inb(PS2_DATA_PORT);
    }

    // Register handlers
    idt_register_interrupt_handler(IDT_VECTOR_PS2_KEYBOARD, keyboard_irq_handler);
    if (dual_channel) idt_register_interrupt_handler(IDT_VECTOR_PS2_MOUSE, mouse_irq_handler);

    // Configure IOAPIC
    ioapic_set_irq(PS2_IRQ_KEYBOARD, 0, IDT_VECTOR_PS2_KEYBOARD);
    if (dual_channel) ioapic_set_irq(PS2_IRQ_MOUSE, 0, IDT_VECTOR_PS2_MOUSE);

    // Enable IRQ
    config |= PS2_CONFIG_PORT1_INT;
    if (dual_channel)
    {
        config |= PS2_CONFIG_PORT2_INT;
    }

    if (!ps2_wait_write()) return false;
    outb(PS2_COMMAND_PORT, PS2_CMD_WRITE_CONFIG);
    if (!ps2_wait_write()) return false;
    outb(PS2_DATA_PORT, config);

    kprintf("Done.\n");
    return true;
}

bool ps2_poll_keyboard(keyboard_event_t* out_event)
{
    uint8_t scancode;
    key_code_t key_code;
    bool pressed = false;

    while (spsc_ring_buffer_pop(ring_buffer, &scancode)) {
        if (handle_keyboard_scancode(scancode, &key_code, &pressed)) {
            out_event->key_code = key_code;
            out_event->pressed = pressed;
            process_modifers(out_event);
            out_event->modifiers = modifiers;
            return true;
        }
    }

    return false;
}