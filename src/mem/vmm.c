#include <kernel/panic.h>
#include <kernel/arch/io.h>
#include <kernel/lib/string.h>
#include <kernel/mem/pmm.h>
#include <kernel/mem/vmm.h>

static inline uint64_t read_cr3(void)
{
    uint64_t cr3_val;
    // 'volatile' prevents the compiler from optimizing this away
    // '=r' tells the compiler to put the result in a general-purpose register
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3_val));
    return cr3_val;
}

static uint64_t* get_or_allocate_table(uint64_t* current_table, uint32_t index)
{
    if (current_table[index] & PAGE_PRESENT)
    {
        if (current_table[index] & PAGE_HUGE)
        {
            // Split the 2MB huge page into 512 x 4KB pages
            uint64_t huge_phys = current_table[index] & PHYS_ADDR_MASK;
            uint64_t huge_flags = current_table[index] & ~PHYS_ADDR_MASK;
            huge_flags &= ~PAGE_HUGE;

            phys_addr_t new_table_phys = pmm_alloc_frame();
            if (new_table_phys == 0) return 0;

            virt_addr_t* new_table_virt = (virt_addr_t*)PHYS_TO_VIRT(new_table_phys);
            for (int i = 0; i < HUGE_PAGE_FRAME_COUNT; i++)
            {
                new_table_virt[i] = (huge_phys + (uint64_t)i * PAGE_SIZE) | huge_flags | PAGE_PRESENT;
            }

            // Replace huge page with pointer to new page table
            current_table[index] = new_table_phys | PAGE_PRESENT | PAGE_WRITABLE;

            // Flush TLB to ensure the huge page mapping is removed from the TLB
            uint64_t cr3 = read_cr3();
            __asm__ volatile("mov %0, %%cr3" :: "r"(cr3) : "memory");
        }

        phys_addr_t next_table_phys = current_table[index] & PHYS_ADDR_MASK;
        // Reach page tables through the Direct Map (512GiB window)
        return (virt_addr_t*)PHYS_TO_VIRT(next_table_phys);
    }

    phys_addr_t new_table_phys = pmm_alloc_frame();
    if (new_table_phys == 0)
    {
        return 0;
    }

    virt_addr_t* new_table_virt = (virt_addr_t*)PHYS_TO_VIRT(new_table_phys);
    memset(new_table_virt, 0, 4096);

    current_table[index] = new_table_phys | PAGE_PRESENT | PAGE_WRITABLE;

    return new_table_virt;
}

static void map_page_2mb_in(virt_addr_t* pml4_virt, phys_addr_t phys_addr, virt_addr_t virt_addr, uint64_t flags)
{
    uint32_t pml4_idx = PML4_GET_INDEX(virt_addr);
    uint32_t pdpt_idx = PDPT_GET_INDEX(virt_addr);
    uint32_t pd_idx   = PD_GET_INDEX(virt_addr);

    virt_addr_t* pdpt = get_or_allocate_table(pml4_virt, pml4_idx);
    virt_addr_t* pd   = get_or_allocate_table(pdpt, pdpt_idx);

    if (pdpt == 0 || pd == 0)
    {
        kernel_panic("Out of memory", NULL);
        return;
    }

    // Map the 2MB physical page with supplied flags + HUGE bit
    pd[pd_idx] = (phys_addr & ~(HUGE_PAGE_SIZE - 1)) | flags | PAGE_PRESENT | PAGE_HUGE;

    // Invalidate single page in TLB instead of reloading entire CR3
    __asm__ volatile("invlpg (%0)" :: "r"(virt_addr) : "memory");
}

static void map_page_4k_in(virt_addr_t* pml4_virt, phys_addr_t phys_addr, virt_addr_t virt_addr, uint64_t flags)
{
    uint32_t pml4_idx = PML4_GET_INDEX(virt_addr);
    uint32_t pdpt_idx = PDPT_GET_INDEX(virt_addr);
    uint32_t pd_idx   = PD_GET_INDEX(virt_addr);
    uint32_t pt_idx   = PT_GET_INDEX(virt_addr);

    virt_addr_t* pdpt = get_or_allocate_table(pml4_virt, pml4_idx);
    virt_addr_t* pd   = get_or_allocate_table(pdpt, pdpt_idx);
    virt_addr_t* pt   = get_or_allocate_table(pd, pd_idx);

    if (pdpt == 0 || pd == 0 || pt == 0)
    {
        kernel_panic("Out of memory", NULL);
        return;
    }

    // Map the 4KB physical page with supplied flags
    pt[pt_idx] = (phys_addr & PHYS_ADDR_MASK) | flags | PAGE_PRESENT;

    // Invalidate single page in TLB
    __asm__ volatile("invlpg (%0)" :: "r"(virt_addr) : "memory");
}

static inline virt_addr_t* current_pml4(void)
{
    phys_addr_t pml4_phys = read_cr3() & PHYS_ADDR_MASK;
    return (virt_addr_t*)PHYS_TO_VIRT(pml4_phys);
}

void unmap_identity_map()
{
    pml4[0] = 0;

    // Flush TLB by reloading CR3
    uint64_t cr3;
    __asm__ __volatile__ ("mov %%cr3, %0" : "=r"(cr3));
    __asm__ __volatile__ ("mov %0, %%cr3" : : "r"(cr3) : "memory");
}

void vmm_init()
{
    uint64_t pat = rdmsr(IA32_PAT_MSR);

    // Set PAT slot 2 Write Combining
    pat &= ~PAT_MASK(PAT_SLOT_WC);
    pat |= PAT_ENTRY(PAT_SLOT_WC, PAT_TYPE_WC);

    wrmsr(IA32_PAT_MSR, pat);

    virt_addr_t* pml4_virt = current_pml4();

    // The boot direct map already covers the low 4GiB (pd0..pd3), so only map
    // whatever RAM exists above that.
    phys_addr_t phys_addr = 0x100000000ULL; // 4GiB
    virt_addr_t virt_addr = DIRECT_MAP_BASE + phys_addr;

    // Loop through all physical RAM detected by PMM and map into DIRECT_MAP_BASE
    while (phys_addr < pmm_max_phys_addr)
    {
        map_page_2mb_in(pml4_virt, phys_addr, virt_addr, PAGE_WRITABLE);
        phys_addr += HUGE_PAGE_SIZE;
        virt_addr += HUGE_PAGE_SIZE;
    }

    // Reload CR3 to flush entire TLB after bulk initialization
    uint64_t cr3 = read_cr3();
    __asm__ volatile("mov %0, %%cr3" :: "r"(cr3) : "memory");
}

void vmm_map_page_2mb(phys_addr_t phys_addr, virt_addr_t virt_addr, uint64_t flags)
{
    map_page_2mb_in(current_pml4(), phys_addr, virt_addr, flags);
}

void vmm_map_page_4kb(phys_addr_t phys_addr, virt_addr_t virt_addr, uint64_t flags)
{
    map_page_4k_in(current_pml4(), phys_addr, virt_addr, flags);
}

void vmm_map_range(phys_addr_t phys_addr, virt_addr_t virt_addr, size_t size, uint64_t flags)
{
    uint64_t mapped = 0;

    while (mapped < size)
    {
        uint64_t remaining = size - mapped;
        phys_addr_t curr_phys = phys_addr + mapped;
        virt_addr_t curr_virt = virt_addr + mapped;

        if (remaining >= HUGE_PAGE_SIZE && IS_ALIGNED(curr_phys, HUGE_PAGE_SIZE))
        {
            vmm_map_page_2mb(curr_phys, curr_virt, flags);
            mapped += HUGE_PAGE_SIZE;
        } else
        {
            vmm_map_page_4kb(curr_phys, curr_virt, flags);
            mapped += PAGE_SIZE;
        }
    }
}

void vmm_alloc_map_range(virt_addr_t virt_addr, size_t size, uint64_t flags)
{
    uint64_t mapped = 0;

    while (mapped < size)
    {
        uint64_t remaining = size - mapped;
        virt_addr_t curr_virt = virt_addr + mapped;

        if (remaining >= HUGE_PAGE_SIZE && IS_ALIGNED(curr_virt, HUGE_PAGE_SIZE))
        {
            phys_addr_t phys = pmm_alloc_huge_frame();
            if (phys)
            {
                vmm_map_page_2mb(phys, curr_virt, flags);
                mapped += HUGE_PAGE_SIZE;
                continue;
            }
        }

        phys_addr_t phys = pmm_alloc_frame();
        if (phys)
        {
            vmm_map_page_4kb(phys, curr_virt, flags);
            mapped += PAGE_SIZE;
        }
        else
        {
            kernel_panic("Out of memory", NULL);
            return;
        }
    }
}