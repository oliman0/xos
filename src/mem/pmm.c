#include <kernel/mem/pmm.h>
#include <kernel/mem/vmm.h>

uint64_t pmm_max_phys_addr = 0;

static uint8_t *bitmap;
static uint64_t total_frames;

void clear_bss(void) {
    uint8_t* bss = _bss_start;
    while (bss < _bss_end) {
        *bss++ = 0;
    }
}

void unmap_identity_map() {
    pml4[0] = 0; // Linker handles the virtual address resolution

    // Flush TLB
    __asm__ __volatile__ (
        "mov %%cr3, %%rax\n\t"
        "mov %%rax, %%cr3"
        : : : "rax", "memory"
    );
}

void pmm_init(uint64_t multiboot2_info_addr, multiboot_tag_mmap_t* mmap_tag)
{
    uint64_t max_addr = 0;
    uint32_t num_entries = (mmap_tag->size - sizeof(multiboot_tag_mmap_t)) / mmap_tag->entry_size;

    for (uint32_t i = 0; i < num_entries; i++)
    {
        uint64_t entry_addr = (uint64_t)mmap_tag->entries + (i * mmap_tag->entry_size);
        multiboot_mmap_entry_t *entry = (multiboot_mmap_entry_t *)entry_addr;

        if (entry->addr + entry->len > max_addr)
        {
            max_addr = entry->addr + entry->len;
        }
    }

    // Calculate total frames & required bitmap bytes
    total_frames = max_addr / PAGE_SIZE;
    uint64_t bitmap_size_bytes = (total_frames + 7) / 8;

    // Convert kernel_end to PHYSICAL address for calculations
    uint64_t kernel_end_phys = (uint64_t)&_kernel_end - KERNEL_VMA;

    // Multiboot info address is VIRTUAL
    uint64_t mb2_start_phys = multiboot2_info_addr - KERNEL_VMA;
    uint32_t mb2_total_size = *(uint32_t*)multiboot2_info_addr;
    uint64_t mb2_end_phys = mb2_start_phys + mb2_total_size;

    // Work strictly in physical addresses
    uint64_t bitmap_start_phys = kernel_end_phys;

    if (bitmap_start_phys < mb2_end_phys)
    {
        bitmap_start_phys = mb2_end_phys;
    }

    // Page-align the physical start of the bitmap
    bitmap_start_phys = (bitmap_start_phys + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    // Reach the bitmap through the Direct Map so it is valid regardless of how
    // large it is (the KERNEL_VMA window is only 2GiB; the direct map is 512GiB
    // and covers the low 4GiB from boot).
    bitmap = (uint8_t *)PHYS_TO_VIRT(bitmap_start_phys);

    // Initialize all memory to 1s (Reserved / In-Use)
    memset(bitmap, 0xFF, bitmap_size_bytes);

    // Free available memory regions (using physical addresses)
    for (uint32_t i = 0; i < num_entries; i++)
    {
        uint64_t entry_addr = (uint64_t)mmap_tag->entries + (i * mmap_tag->entry_size);
        multiboot_mmap_entry_t *entry = (multiboot_mmap_entry_t *)entry_addr;

        if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
            pmm_free_region(entry->addr, entry->len);
        }
    }

    // Reserve lower memory up through the bitmap (using physical addresses)
    uint64_t reserved_end_phys = bitmap_start_phys + bitmap_size_bytes;
    pmm_reserve_region(0, reserved_end_phys);

    pmm_max_phys_addr = max_addr;
}

void pmm_free_region(uint64_t base_addr, uint64_t size) {
    // Ensure we start on a 4KB aligned boundary
    uint64_t start_frame = (base_addr + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t end_frame = (base_addr + size) / PAGE_SIZE;

    for (uint64_t i = start_frame; i < end_frame; i++) {
        BITMAP_CLEAR(i); // 0 means FREE
    }
}

static uint64_t last_alloc_index = 0;

void* pmm_alloc_frame() {
    for (uint64_t i = last_alloc_index; i < total_frames; i++) {
        if (!BITMAP_TEST(i)) {
            BITMAP_SET(i);
            last_alloc_index = i;
            return (void*)(i * PAGE_SIZE);
        }
    }

    for (uint64_t i = 0; i < last_alloc_index; i++) {
        if (!BITMAP_TEST(i)) {
            BITMAP_SET(i);
            last_alloc_index = i;
            return (void*)(i * PAGE_SIZE);
        }
    }

    return NULL;
}

void pmm_free_frame(uint64_t physical_addr) {
    uint64_t frame_index = physical_addr / PAGE_SIZE;

    if (frame_index < total_frames) {
        BITMAP_CLEAR(frame_index);

        // Optimization: move our search hint backwards if we freed lower memory
        if (frame_index < last_alloc_index) {
            last_alloc_index = frame_index;
        }
    }
}

void pmm_reserve_frame(uint64_t physical_addr)
{
    uint64_t frame = physical_addr / PAGE_SIZE;
    uint64_t byte_idx = frame / 8;
    uint8_t bit_idx = frame % 8;

    // Set the bit to 1 using a bitwise OR
    bitmap[byte_idx] |= (1 << bit_idx);
}

void pmm_reserve_region(uint64_t start_addr, uint64_t size)
{
    // Round down to the nearest frame boundary
    uint64_t start_frame = start_addr / PAGE_SIZE;

    // Round up to the nearest frame boundary for the end address
    uint64_t end_frame = (start_addr + size + PAGE_SIZE - 1) / PAGE_SIZE;

    for (uint64_t i = start_frame; i < end_frame; i++) {
        BITMAP_SET(i);
    }
}