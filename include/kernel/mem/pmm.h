#ifndef XOS_PMM_H
#define XOS_PMM_H

#include <stdint.h>

#include <kernel/boot/multiboot2.h>

#define PAGE_SIZE 0x1000
#define HUGE_PAGE_SIZE 0x200000

#define KERNEL_VMA 0xFFFFFFFF80000000ULL
#define PHYS_ADDR_MASK 0x000FFFFFFFFFF000ULL

// Bitwise helper macros
// Index / 8 gives the byte. Index % 8 gives the bit inside that byte.
#define BITMAP_SET(bit)   (bitmap[(bit) / 8] |=  (1 << ((bit) % 8)))
#define BITMAP_CLEAR(bit) (bitmap[(bit) / 8] &= ~(1 << ((bit) % 8)))
#define BITMAP_TEST(bit)  (bitmap[(bit) / 8] &   (1 << ((bit) % 8)))

extern uint8_t _bss_start[];
extern uint8_t _bss_end[];

extern uint8_t _kernel_end;

extern uint64_t pmm_max_phys_addr;

void pmm_init(uint64_t multiboot2_info_addr, multiboot_tag_mmap_t* mmap_tag);

void* pmm_alloc_frame();
void pmm_free_frame(uint64_t physical_addr);

void pmm_reserve_frame(uint64_t physical_addr);
void pmm_reserve_region(uint64_t start_addr, uint64_t size);

void pmm_free_region(uint64_t base_addr, uint64_t size);

#endif