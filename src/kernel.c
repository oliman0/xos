#include <kernel/boot/multiboot2.h>
#include <kernel/kernel_io.h>
#include <kernel/mem/pmm.h>
#include <kernel/mem/vmm.h>
#include <kernel/drivers/uefi_linear_framebuffer.h>

void kernel_main(uint64_t multiboot2_info_addr)
{
    clear_bss();

    unmap_identity_map();

    multiboot_info_table_t info_table = parse_multiboot2_tags(multiboot2_info_addr);

    pmm_init(multiboot2_info_addr, info_table.mmap_tag);
    vmm_init();

    fb_init(info_table.framebuffer_tag);

    fb_clear(0x000d1b2a);

    kprintf("Kernel Booted.");

    while (1) {
        // 'hlt' puts the CPU to sleep until the next hardware interrupt fires,
        // saving power instead of spinning the fan at 100%.
        __asm__ volatile ("hlt");
    }
}