#include <kernel/mem/vmm.h>

static inline uint64_t read_cr3(void) {
    uint64_t cr3_val;
    // 'volatile' prevents the compiler from optimizing this away
    // '=r' tells the compiler to put the result in a general-purpose register
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3_val));
    return cr3_val;
}

static uint64_t* get_or_allocate_table(uint64_t* current_table, uint32_t index) {
    // 1. Check if the entry is already present
    if (current_table[index] & PAGE_PRESENT) {
        // Extract the physical address by masking off the bottom 12 flag bits
        uint64_t next_table_phys = current_table[index] & 0xFFFFFFFFFFFFF000;

        // Return as a virtual pointer using our bootloader's higher-half mapping
        return (uint64_t*)(next_table_phys + KERNEL_VMA);
    }

    // 2. Table doesn't exist, ask the PMM for a raw physical frame
    // Note: ensure your pmm_alloc_frame returns a uint64_t physical address
    uint64_t new_table_phys = (uint64_t)pmm_alloc_frame();
    if (new_table_phys == 0) {
        // Out of memory! (In a real OS, you'd want a kernel panic here)
        return 0;
    }

    // 3. Create a virtual pointer to the new frame so C can write to it
    uint64_t* new_table_virt = (uint64_t*)(new_table_phys + KERNEL_VMA);

    // 4. Zero out the new table so we don't have garbage mapping data
    // A single page table is exactly 4096 bytes
    memset(new_table_virt, 0, 4096);

    // 5. Link the new physical table into the current table
    // We set Present and Writable so the kernel can modify anything underneath it
    current_table[index] = new_table_phys | PAGE_PRESENT | PAGE_WRITABLE;

    return new_table_virt;
}

void vmm_init(void) {
    uint64_t phys_addr = 0;
    uint64_t virt_addr = DIRECT_MAP_BASE;

    // Loop through all physical RAM detected by PMM and map into DIRECT_MAP_BASE
    while (phys_addr < pmm_max_phys_addr) {
        vmm_map_page_2mb(phys_addr, virt_addr, PAGE_WRITABLE);

        phys_addr += HUGE_PAGE_SIZE;
        virt_addr += HUGE_PAGE_SIZE;
    }

    // Reload CR3 to flush entire TLB after bulk initialization
    uint64_t cr3 = read_cr3();
    __asm__ volatile("mov %0, %%cr3" :: "r"(cr3) : "memory");
}

void vmm_map_page_2mb(uint64_t phys_addr, uint64_t virt_addr, uint64_t flags) {
    uint64_t pml4_phys = read_cr3() & 0xFFFFFFFFFFFFF000;
    uint64_t* pml4 = (uint64_t*)(pml4_phys + KERNEL_VMA);

    uint32_t pml4_idx = PML4_GET_INDEX(virt_addr);
    uint32_t pdpt_idx = PDPT_GET_INDEX(virt_addr);
    uint32_t pd_idx   = PD_GET_INDEX(virt_addr);

    uint64_t* pdpt = get_or_allocate_table(pml4, pml4_idx);
    uint64_t* pd   = get_or_allocate_table(pdpt, pdpt_idx);

    // Map the 2MB physical page with supplied flags + HUGE bit
    pd[pd_idx] = (phys_addr & ~0x1FFFFFULL) | flags | PAGE_PRESENT | PAGE_HUGE;

    // Invalidate single page in TLB instead of reloading entire CR3
    __asm__ volatile("invlpg (%0)" :: "r"(virt_addr) : "memory");
}

void vmm_map_range(uint64_t phys_addr, uint64_t virt_addr, uint64_t size, uint64_t flags) {
    uint64_t page_offset = 0;

    while (page_offset < size) {
        vmm_map_page_2mb(phys_addr + page_offset, virt_addr + page_offset, flags);
        page_offset += HUGE_PAGE_SIZE; // 2MB steps
    }
}