#include <stdbool.h>
#include <kernel/lib/string.h>
#include <kernel/mem/pmm.h>
#include <kernel/mem/vmm.h>

phys_addr_t pmm_max_phys_addr = 0;

static uint8_t *bitmap;
static uint64_t total_frames;

void pmm_init(virt_addr_t multiboot2_info_addr, multiboot_tag_mmap_t* mmap_tag)
{
    phys_addr_t max_addr = 0;
    uint32_t num_entries = (mmap_tag->size - sizeof(multiboot_tag_mmap_t)) / mmap_tag->entry_size;

    for (uint32_t i = 0; i < num_entries; i++)
    {
        phys_addr_t entry_addr = (phys_addr_t)mmap_tag->entries + (i * mmap_tag->entry_size);
        multiboot_mmap_entry_t *entry = (multiboot_mmap_entry_t *)entry_addr;

        if (entry->addr + entry->len > max_addr)
        {
            max_addr = entry->addr + entry->len;
        }
    }

    // Calculate total frames & required bitmap bytes
    total_frames = max_addr / PAGE_SIZE;
    size_t bitmap_size_bytes = (total_frames + 7) / 8;

    // Convert kernel_end to PHYSICAL address for calculations
    phys_addr_t kernel_end_phys = (phys_addr_t)&_kernel_end - KERNEL_VMA;

    // Multiboot info address is VIRTUAL
    phys_addr_t mb2_start_phys = multiboot2_info_addr - KERNEL_VMA;
    phys_addr_t mb2_total_size = *(uint32_t*)multiboot2_info_addr;
    phys_addr_t mb2_end_phys = mb2_start_phys + mb2_total_size;

    // Work strictly in physical addresses
    phys_addr_t bitmap_start_phys = kernel_end_phys;

    if (bitmap_start_phys < mb2_end_phys)
    {
        bitmap_start_phys = mb2_end_phys;
    }

    bitmap_start_phys = ALIGN_UP(bitmap_start_phys, PAGE_SIZE);

    // Reach the bitmap through the Direct Map so it is valid regardless of how
    // large it is (the KERNEL_VMA window is only 2GiB; the direct map is 512GiB
    // and covers the low 4GiB from boot).
    bitmap = (uint8_t *)PHYS_TO_VIRT(bitmap_start_phys);

    // Initialize all memory to 1s (Reserved / In-Use)
    memset(bitmap, 0xFF, bitmap_size_bytes);

    // Free available memory regions (using physical addresses)
    for (uint32_t i = 0; i < num_entries; i++)
    {
        phys_addr_t entry_addr = (phys_addr_t)mmap_tag->entries + (i * mmap_tag->entry_size);
        multiboot_mmap_entry_t *entry = (multiboot_mmap_entry_t *)entry_addr;

        if (entry->type == MULTIBOOT_MEMORY_AVAILABLE)
        {
            pmm_free_region(entry->addr, entry->len);
        }
    }

    // Reserve lower memory up through the bitmap (using physical addresses)
    phys_addr_t reserved_end_phys = bitmap_start_phys + bitmap_size_bytes;
    pmm_reserve_region(0, reserved_end_phys);

    pmm_max_phys_addr = max_addr;
}

static uint64_t last_alloc_index = 0;

phys_addr_t pmm_alloc_frame()
{
    for (uint64_t i = last_alloc_index; i < total_frames; i++)
    {
        if (!BITMAP_TEST(i))
        {
            BITMAP_SET(i);
            last_alloc_index = i;
            return (i * PAGE_SIZE);
        }
    }

    for (uint64_t i = 0; i < last_alloc_index; i++)
    {
        if (!BITMAP_TEST(i))
        {
            BITMAP_SET(i);
            last_alloc_index = i;
            return (i * PAGE_SIZE);
        }
    }

    return (phys_addr_t)NULL;
}

phys_addr_t pmm_alloc_huge_frame()
{
    uint64_t start_index = ALIGN_UP(last_alloc_index, HUGE_PAGE_FRAME_COUNT);

    for (uint64_t i = start_index; i + HUGE_PAGE_FRAME_COUNT - 1 < total_frames; i += HUGE_PAGE_FRAME_COUNT)
    {
        bool all_free = true;
        for (uint64_t j = 0; j < HUGE_PAGE_FRAME_COUNT; j++)
        {
            if (BITMAP_TEST(i + j))
            {
                all_free = false;
                break;
            }
        }

        if (all_free)
        {
            for (uint64_t j = 0; j < HUGE_PAGE_FRAME_COUNT; j++)
            {
                BITMAP_SET(i + j);
            }
            last_alloc_index = i;
            return (i * PAGE_SIZE);
        }
    }

    for (uint64_t i = 0; i < start_index; i += HUGE_PAGE_FRAME_COUNT)
    {
        // Ensure we don't overflow total_frames in case start_index was near the end
        if (i + HUGE_PAGE_FRAME_COUNT - 1 >= total_frames) break;

        bool all_free = true;
        for (uint64_t j = 0; j < HUGE_PAGE_FRAME_COUNT; j++)
        {
            if (BITMAP_TEST(i + j))
            {
                all_free = false;
                break;
            }
        }

        if (all_free)
        {
            for (uint64_t j = 0; j < HUGE_PAGE_FRAME_COUNT; j++)
            {
                BITMAP_SET(i + j);
            }
            last_alloc_index = i;
            return i * PAGE_SIZE;
        }
    }

    return (phys_addr_t)NULL;
}

void pmm_free_frame(phys_addr_t physical_addr)
{
    uint64_t frame_index = physical_addr / PAGE_SIZE;

    if (frame_index < total_frames)
    {
        BITMAP_CLEAR(frame_index);

        if (frame_index < last_alloc_index)
        {
            last_alloc_index = frame_index;
        }
    }
}

void pmm_reserve_frame(phys_addr_t physical_addr)
{
    uint64_t frame_index = physical_addr / PAGE_SIZE;

    if (frame_index < total_frames)
    {
        BITMAP_SET(frame_index);

        if (frame_index > last_alloc_index)
        {
            last_alloc_index = frame_index;
        }
    }
}

void pmm_free_region(phys_addr_t physical_addr, size_t size)
{
    uint64_t start_frame = ALIGN_UP(physical_addr, PAGE_SIZE) / PAGE_SIZE;
    uint64_t end_frame = (physical_addr + size) / PAGE_SIZE;

    if (start_frame >= end_frame) return;

    if (end_frame > total_frames) end_frame = total_frames;

    for (uint64_t i = start_frame; i < end_frame; i++)
    {
        BITMAP_CLEAR(i);
    }

    if (start_frame < last_alloc_index)
    {
        last_alloc_index = start_frame;
    }
}

void pmm_reserve_region(phys_addr_t physical_addr, size_t size)
{
    // Round down to the nearest frame boundary
    uint64_t start_frame = physical_addr / PAGE_SIZE;
    uint64_t end_frame = ALIGN_UP(physical_addr + size, PAGE_SIZE) / PAGE_SIZE;

    if (start_frame >= end_frame) return;

    if (end_frame > total_frames) end_frame = total_frames;

    for (uint64_t i = start_frame; i < end_frame; i++)
    {
        BITMAP_SET(i);
    }

    if (end_frame > last_alloc_index)
    {
        last_alloc_index = end_frame;
    }
}