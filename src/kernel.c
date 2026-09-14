#include <kernel/boot/multiboot2.h>
#include <kernel/kernel_io.h>
#include <kernel/mem/pmm.h>
#include <kernel/mem/vmm.h>
#include <kernel/gdt.h>
#include <kernel/idt.h>
#include <kernel/scheduler.h>
#include <kernel/drivers/lapic.h>
#include <kernel/drivers/ioapic.h>
#include <kernel/drivers/pic.h>
#include <kernel/drivers/ps2.h>
#include <kernel/drivers/linear_framebuffer.h>
#include <kernel/mem/heap.h>
#include <kernel/drivers/acpi.h>

extern uint8_t _bss_start[];
extern uint8_t _bss_end[];

void clear_bss(void)
{
    uint8_t* bss = _bss_start;
    while (bss < _bss_end) {
        *bss++ = 0;
    }
}

void kernel_main(uint64_t multiboot2_info_addr)
{
    clear_bss();

    gdt_init();
    idt_init();

    pic_disable();

    unmap_identity_map();

    multiboot_tags_t info_table = parse_multiboot2_tags(multiboot2_info_addr);

    pmm_init(multiboot2_info_addr, info_table.mmap_tag);
    vmm_init();
    heap_init();

    scheduler_init();

    fb_init(info_table.framebuffer_tag);

    fb_clear(0x000d1b2a);
    fb_set_font_scale(2);

    acpi_init(info_table.acpi_tag);

    lapic_init();

    ioapic_init(info_table.acpi_tag);

    uint32_t ticks_per_ms = lapic_get_ticks_per_ms();

    kprintf("Timer tpms: %d\n", ticks_per_ms);

    lapic_timer_start_periodic(1000, ticks_per_ms, IDT_VECTOR_TIMER);

    if (!ps2_init())
    {
        kprintf("Failed to initialise PS/2.\n");
    }

    kprintf("Kernel Booted.\n");

    keyboard_event_t event;
    while (1)
    {
        while (ps2_poll_keyboard(&event))
        {
            if (event.pressed) kprintf("%c", ascii_base_map[event.key_code]);
        }

        fb_swap_buffers();

        __asm__ __volatile__ ("hlt");
    }
}