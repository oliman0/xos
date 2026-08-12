#include <kernel/mem/vmm.h>

static inline uint64_t read_cr3(void) {
    uint64_t cr3_val;
    // 'volatile' prevents the compiler from optimizing this away
    // '=r' tells the compiler to put the result in a general-purpose register
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3_val));
    return cr3_val;
}

static uint64_t* get_or_allocate_table(uint64_t* current_table, uint32_t index) {
    if (current_table[index] & PAGE_PRESENT) {
        uint64_t next_table_phys = current_table[index] & PHYS_ADDR_MASK;
        // Reach page tables through the Direct Map (512GiB window), not the
        // 2GiB KERNEL_VMA window. The boot direct map covers the low 4GiB,
        // and every table frame we allocate here is itself low, so this is
        // always resolvable.
        return (uint64_t*)PHYS_TO_VIRT(next_table_phys);
    }

    uint64_t new_table_phys = (uint64_t)pmm_alloc_frame();
    if (new_table_phys == 0) {
        return 0;
    }

    uint64_t* new_table_virt = (uint64_t*)PHYS_TO_VIRT(new_table_phys);
    memset(new_table_virt, 0, 4096);

    current_table[index] = new_table_phys | PAGE_PRESENT | PAGE_WRITABLE;

    return new_table_virt;
}

static void map_page_2mb_in(uint64_t* pml4_virt, uint64_t phys_addr, uint64_t virt_addr, uint64_t flags) {
    uint32_t pml4_idx = PML4_GET_INDEX(virt_addr);
    uint32_t pdpt_idx = PDPT_GET_INDEX(virt_addr);
    uint32_t pd_idx   = PD_GET_INDEX(virt_addr);

    uint64_t* pdpt = get_or_allocate_table(pml4_virt, pml4_idx);
    uint64_t* pd   = get_or_allocate_table(pdpt, pdpt_idx);

    // Map the 2MB physical page with supplied flags + HUGE bit
    pd[pd_idx] = (phys_addr & ~0x1FFFFFULL) | flags | PAGE_PRESENT | PAGE_HUGE;

    // Invalidate single page in TLB instead of reloading entire CR3
    __asm__ volatile("invlpg (%0)" :: "r"(virt_addr) : "memory");
}

static inline uint64_t* current_pml4(void) {
    uint64_t pml4_phys = read_cr3() & PHYS_ADDR_MASK;
    return (uint64_t*)PHYS_TO_VIRT(pml4_phys);
}

void vmm_init(void) {
    uint64_t* pml4_virt = current_pml4();

    // The boot direct map already covers the low 4GiB (pd0..pd3), so only map
    // whatever RAM exists above that.
    uint64_t phys_addr = 0x100000000ULL; // 4GiB
    uint64_t virt_addr = DIRECT_MAP_BASE + phys_addr;

    // Loop through all physical RAM detected by PMM and map into DIRECT_MAP_BASE
    while (phys_addr < pmm_max_phys_addr) {
        map_page_2mb_in(pml4_virt, phys_addr, virt_addr, PAGE_WRITABLE);
        phys_addr += HUGE_PAGE_SIZE;
        virt_addr += HUGE_PAGE_SIZE;
    }

    // Reload CR3 to flush entire TLB after bulk initialization
    uint64_t cr3 = read_cr3();
    __asm__ volatile("mov %0, %%cr3" :: "r"(cr3) : "memory");
}

void vmm_map_page_2mb(uint64_t phys_addr, uint64_t virt_addr, uint64_t flags) {
    map_page_2mb_in(current_pml4(), phys_addr, virt_addr, flags);
}

void vmm_map_range(uint64_t phys_addr, uint64_t virt_addr, uint64_t size, uint64_t flags) {
    uint64_t page_offset = 0;

    while (page_offset < size) {
        vmm_map_page_2mb(phys_addr + page_offset, virt_addr + page_offset, flags);
        page_offset += HUGE_PAGE_SIZE; // 2MB steps
    }
}