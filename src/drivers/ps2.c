#include <kernel/drivers/ps2.h>
#include <kernel/drivers/ioapic.h>
#include <kernel/arch/io.h>
#include <kernel/idt.h>
#include <kernel/kernel_io.h>
#include <kernel/drivers/lapic.h>
#include <stdbool.h>
#include <kernel/drivers/linear_framebuffer.h>

static bool lshift = false;
static bool rshift = false;

static unsigned char kbd_us[128] =
{
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8',	/* 9 */
  '9', '0', '-', '=', '\b',	/* Backspace */
  '\t',			/* Tab */
  'q', 'w', 'e', 'r',	/* 19 */
  't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',	/* Enter key */
    0,			/* 29   - Control */
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',	/* 39 */
 '\'', '`',   0,		/* Left shift */
 '\\', 'z', 'x', 'c', 'v', 'b', 'n',			/* 49 */
  'm', ',', '.', '/',   0,				/* Right shift */
  '*',
    0,	/* Alt */
  ' ',	/* Space bar */
    0,	/* Caps lock */
    0,	/* 59 - F1 key ... > */
    0,   0,   0,   0,   0,   0,   0,   0,
    0,	/* < ... F10 */
    0,	/* 69 - Num lock*/
    0,	/* Scroll Lock */
    0,	/* Home key */
    0,	/* Up Arrow */
    0,	/* Page Up */
  '-',
    0,	/* Left Arrow */
    0,
    0,	/* Right Arrow */
  '+',
    0,	/* 79 - End key*/
    0,	/* Down Arrow */
    0,	/* Page Down */
    0,	/* Insert Key */
    0,	/* Delete Key */
    0,   0,   0,
    0,	/* F11 Key */
    0,	/* F12 Key */
    0,	/* All other keys are 0 */
};

static unsigned char kbd_us_shift[128] =
{
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*',	/* 9 */
  '(', ')', '_', '+', '\b',	/* Backspace */
  '\t',			/* Tab */
  'Q', 'W', 'E', 'R',	/* 19 */
  'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',	/* Enter key */
    0,			/* 29   - Control */
  'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',	/* 39 */
 '\"', '~',   0,		/* Left shift */
 '|', 'Z', 'X', 'C', 'V', 'B', 'N',			/* 49 */
  'M', '<', '>', '?',   0,				/* Right shift */
  '*',
    0,	/* Alt */
  ' ',	/* Space bar */
    0,	/* Caps lock */
    0,	/* 59 - F1 key ... > */
    0,   0,   0,   0,   0,   0,   0,   0,
    0,	/* < ... F10 */
    0,	/* 69 - Num lock*/
    0,	/* Scroll Lock */
    0,	/* Home key */
    0,	/* Up Arrow */
    0,	/* Page Up */
  '-',
    0,	/* Left Arrow */
    0,
    0,	/* Right Arrow */
  '+',
    0,	/* 79 - End key*/
    0,	/* Down Arrow */
    0,	/* Page Down */
    0,	/* Insert Key */
    0,	/* Delete Key */
    0,   0,   0,
    0,	/* F11 Key */
    0,	/* F12 Key */
    0,	/* All other keys are 0 */
};

static bool ps2_wait_read() {
    int timeout = PS2_IO_TIMEOUT;
    while (!(inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL) && timeout-- > 0);
    return timeout > 0;
}

static bool ps2_wait_write() {
    int timeout = PS2_IO_TIMEOUT;
    while ((inb(PS2_STATUS_PORT) & PS2_STATUS_INPUT_FULL) && timeout-- > 0);
    return timeout > 0;
}

static char handle_keyboard_scancode(uint8_t scancode) {
    if (scancode == PS2_KBD_LSHIFT) {
        lshift = true;
        return 0;
    } else if (scancode == (PS2_KBD_LSHIFT | PS2_KBD_RELEASE)) {
        lshift = false;
        return 0;
    } else if (scancode == PS2_KBD_RSHIFT) {
        rshift = true;
        return 0;
    } else if (scancode == (PS2_KBD_RSHIFT | PS2_KBD_RELEASE)) {
        rshift = false;
        return 0;
    }

    if (scancode & PS2_KBD_RELEASE) {
        return 0;
    }

    if (scancode < 128) {
        return (lshift || rshift) ? kbd_us_shift[scancode] : kbd_us[scancode];
    }

    return 0;
}

static void keyboard_irq_handler(registers_t* regs) {
    uint8_t scancode = inb(PS2_DATA_PORT);

    char c = handle_keyboard_scancode(scancode);
    if (c)
    {
        kprintf("%c", c);
    }

    lapic_eoi();
}

static void mouse_irq_handler(registers_t* regs) {
    uint8_t data = inb(PS2_DATA_PORT);

    lapic_eoi();
}

bool ps2_init() {
    // Check if PS/2 controller exists (basic check)
    // If status register is 0xFF, it likely doesn't exist
    if (inb(PS2_STATUS_PORT) == 0xFF)
    {
        kprintf("PS/2 Controller not found. ");
        return false;
    }

    // Disable devices
    outb(PS2_COMMAND_PORT, PS2_CMD_DISABLE_PORT1);
    io_wait();
    outb(PS2_COMMAND_PORT, PS2_CMD_DISABLE_PORT2);
    io_wait();

    // Flush output buffer
    int flush_timeout = PS2_IO_TIMEOUT;
    while ((inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL) && flush_timeout-- > 0) {
        inb(PS2_DATA_PORT);
        io_wait();
    }

    // Get controller configuration byte
    outb(PS2_COMMAND_PORT, PS2_CMD_READ_CONFIG);
    if (!ps2_wait_read()) return false;
    uint8_t config = inb(PS2_DATA_PORT);

    // Check for dual channel
    // Is Port 2 Clock Disable bit set while Port 2 disabled
    bool dual_channel = (config & PS2_CONFIG_PORT2_CLK) != 0;
    
    // Enable interrupts for both ports (if supported) and translation
    config |= PS2_CONFIG_PORT1_INT | PS2_CONFIG_TRANSLATION;
    config &= ~PS2_CONFIG_PORT1_CLK;
    if (dual_channel) {
        config |= PS2_CONFIG_PORT2_INT;
        config &= ~PS2_CONFIG_PORT2_CLK;
    }
    
    // Set controller configuration byte
    outb(PS2_COMMAND_PORT, PS2_CMD_WRITE_CONFIG);
    if (!ps2_wait_write()) return false;
    outb(PS2_DATA_PORT, config);
    io_wait();

    // Enable devices
    outb(PS2_COMMAND_PORT, PS2_CMD_ENABLE_PORT1);
    io_wait();
    if (dual_channel) {
        outb(PS2_COMMAND_PORT, PS2_CMD_ENABLE_PORT2);
        io_wait();
    }

    // Register handlers
    idt_register_interrupt_handler(IDT_VECTOR_PS2_KEYBOARD, keyboard_irq_handler);
    if (dual_channel) idt_register_interrupt_handler(IDT_VECTOR_PS2_MOUSE, mouse_irq_handler);

    // Configure IOAPIC
    // We assume the BSP has LAPIC ID 0
    ioapic_set_irq(PS2_IRQ_KEYBOARD, 0, IDT_VECTOR_PS2_KEYBOARD);
    if (dual_channel) ioapic_set_irq(PS2_IRQ_MOUSE, 0, IDT_VECTOR_PS2_MOUSE);

    return true;
}