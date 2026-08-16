#ifndef XOS_PMM_H
#define XOS_PMM_H

#include <stdint.h>
#include <stddef.h>

#include <kernel/boot/multiboot2.h>
#include <kernel/mem/ptr.h>

#define PAGE_SIZE 0x1000
#define HUGE_PAGE_SIZE 0x200000

#define HUGE_PAGE_FRAME_COUNT 512

#define KERNEL_VMA 0xFFFFFFFF80000000ULL
#define PHYS_ADDR_MASK 0x000FFFFFFFFFF000ULL

// Bitwise helper macros
// Index / 8 gives the byte. Index % 8 gives the bit inside that byte.
#define BITMAP_SET(bit)   (bitmap[(bit) / 8] |=  (1 << ((bit) % 8)))
#define BITMAP_CLEAR(bit) (bitmap[(bit) / 8] &= ~(1 << ((bit) % 8)))
#define BITMAP_TEST(bit)  (bitmap[(bit) / 8] &   (1 << ((bit) % 8)))

#define IS_ALIGNED(x, a) (((x) & ((a) - 1)) == 0)
#define ALIGN_UP(x, a) (((x) + ((a) - 1)) & ~((a) - 1))

extern uint8_t _bss_start[];
extern uint8_t _bss_end[];

extern uint8_t _kernel_end;

extern phys_addr_t pmm_max_phys_addr;

void pmm_init(virt_addr_t multiboot2_info_addr, multiboot_tag_mmap_t* mmap_tag);

phys_addr_t pmm_alloc_frame();
phys_addr_t pmm_alloc_huge_frame();

void pmm_reserve_frame(phys_addr_t physical_addr);
void pmm_reserve_region(phys_addr_t start_addr, size_t size);

void pmm_free_frame(phys_addr_t physical_addr);
void pmm_free_region(phys_addr_t base_addr, size_t size);

#endif