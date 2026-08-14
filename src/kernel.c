#include <kernel/boot/multiboot2.h>
#include <kernel/kernel_io.h>
#include <kernel/mem/pmm.h>
#include <kernel/mem/vmm.h>
#include <kernel/gdt.h>
#include <kernel/idt.h>
#include <kernel/drivers/lapic.h>
#include <kernel/drivers/pic.h>
#include <kernel/drivers/uefi_linear_framebuffer.h>

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

    multiboot_info_table_t info_table = parse_multiboot2_tags(multiboot2_info_addr);

    pmm_init(multiboot2_info_addr, info_table.mmap_tag);
    vmm_init();

    fb_init(info_table.framebuffer_tag);
    fb_clear(0x000d1b2a);

    kprintf("Initializing LAPIC... ");
    lapic_init(info_table.acpi_tag);
    kprintf("Done.\n");

    kprintf("Configuring LAPIC timer... ");
    uint32_t ticks_per_ms = get_lapic_ticks_per_ms();
    kprintf("Ticks per ms: %d\n", ticks_per_ms);
    lapic_timer_start_periodic(100, ticks_per_ms, LAPIC_TIMER_VECTOR);
    kprintf("Timer started.\n");

    kprintf("Kernel Booted.");

    while (1)
    {
        // 'hlt' puts the CPU to sleep until the next hardware interrupt fires,
        // saving power instead of spinning the fan at 100%.
        __asm__ volatile ("hlt");
    }
}