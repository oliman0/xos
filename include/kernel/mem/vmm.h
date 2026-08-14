#ifndef XOS_VMM_H
#define XOS_VMM_H

#include <stdint.h>

// The virtual base for the Direct Physical Map (PML4 Entry 256)
#define DIRECT_MAP_BASE 0xFFFF800000000000ULL

// Standard x86_64 Page Table Flags
#define PAGE_PRESENT       (1ull << 0)
#define PAGE_WRITABLE      (1ull << 1)
#define PAGE_USER          (1ull << 2)
#define PAGE_WRITE_THROUGH (1ull << 3)
#define PAGE_CACHE_DISABLE (1ull << 4)
#define PAGE_ACCESSED      (1ull << 5)
#define PAGE_DIRTY         (1ull << 6)
#define PAGE_HUGE          (1ull << 7)
#define PAGE_GLOBAL        (1ull << 8)
#define PAGE_NX            (1ull << 63)

#define PML4_GET_INDEX(addr) (((addr) >> 39) & 0x1FF)
#define PDPT_GET_INDEX(addr) (((addr) >> 30) & 0x1FF)
#define PD_GET_INDEX(addr)   (((addr) >> 21) & 0x1FF)
#define PT_GET_INDEX(addr)   (((addr) >> 12) & 0x1FF)

#define PHYS_TO_VIRT(phys_addr) ((phys_addr) + DIRECT_MAP_BASE)

extern uint64_t pml4[];

void vmm_init();
void unmap_identity_map();

void vmm_map_page_2mb(uint64_t phys_addr, uint64_t virt_addr, uint64_t flags);
void vmm_map_page_4kb(uint64_t phys_addr, uint64_t virt_addr, uint64_t flags);

void vmm_map_range(uint64_t phys_addr, uint64_t virt_addr, uint64_t size, uint64_t flags);

#endif