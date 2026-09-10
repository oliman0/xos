#include <kernel/panic.h>
#include <kernel/arch/io.h>
#include <kernel/lib/string.h>
#include <kernel/mem/pmm.h>
#include <kernel/mem/vmm.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

static uint64_t split_huge_page(uint64_t* current_table, uint32_t index)
{
    if (!(current_table[index] & PAGE_PRESENT) || !(current_table[index] & PAGE_HUGE)) return 0;

    // Split the 2MB huge page into 512 x 4KB pages
    uint64_t huge_phys = current_table[index] & PHYS_ADDR_MASK;
    uint64_t huge_flags = current_table[index] & ~PHYS_ADDR_MASK;
    huge_flags &= ~PAGE_HUGE;

    uint64_t new_table_phys = pmm_alloc_frame();
    if (new_table_phys == 0) return 0;

    uint64_t* new_table_virt = (uint64_t*)PHYS_TO_VIRT(new_table_phys);
    for (int i = 0; i < HUGE_PAGE_FRAME_COUNT; i++)
    {
        new_table_virt[i] = (huge_phys + (uint64_t)i * PAGE_SIZE) | huge_flags | PAGE_PRESENT;
    }

    uint64_t table_flags = PAGE_PRESENT | PAGE_WRITABLE;
    if (huge_flags & PAGE_USER) table_flags |= PAGE_USER;

    // Replace huge page with pointer to new page table
    current_table[index] = new_table_phys | table_flags;

    // Flush TLB to ensure the huge page mapping is removed from the TLB
    uint64_t cr3 = read_cr3();
    __asm__ volatile("mov %0, %%cr3" :: "r"(cr3) : "memory");

    return (uint64_t)new_table_phys;
}

static uint64_t* get_or_allocate_table(uint64_t* current_table, uint32_t index)
{
    if (current_table[index] & PAGE_PRESENT)
    {
        if (current_table[index] & PAGE_HUGE)
        {
            uint64_t split_phys = split_huge_page(current_table, index);
            return split_phys ? (uint64_t*)PHYS_TO_VIRT(split_phys) : NULL;
        }

        uint64_t next_table_phys = current_table[index] & PHYS_ADDR_MASK;
        // Reach page tables through the Direct Map (512GiB window)
        return (uint64_t*)PHYS_TO_VIRT(next_table_phys);
    }

    uint64_t new_table_phys = pmm_alloc_frame();
    if (new_table_phys == 0)
    {
        return 0;
    }

    uint64_t* new_table_virt = (uint64_t*)PHYS_TO_VIRT(new_table_phys);
    memset(new_table_virt, 0, 4096);

    current_table[index] = new_table_phys | PAGE_PRESENT | PAGE_WRITABLE;

    return new_table_virt;
}

static bool table_empty(const uint64_t *table)
{
    for (size_t i = 0; i < PAGE_TABLE_ENTRIES; i++)
    {
        if (table[i] & PAGE_PRESENT)
        {
            return false;
        }
    }

    return true;
}

static bool reclaim_table(uint64_t *table, uint64_t table_phys, uint64_t *parent_table, uint32_t table_idx)
{
    if (!table_empty(table))
    {
        return false;
    }

    parent_table[table_idx] = 0;
    pmm_free_frame(table_phys);
    return true;
}

static void map_page_2mb_in(uint64_t* pml4_virt, uint64_t phys_addr, uint64_t virt_addr, uint64_t flags)
{
    uint32_t pml4_idx = PML4_GET_INDEX(virt_addr);
    uint32_t pdpt_idx = PDPT_GET_INDEX(virt_addr);
    uint32_t pd_idx   = PD_GET_INDEX(virt_addr);

    uint64_t* pdpt = get_or_allocate_table(pml4_virt, pml4_idx);
    if (pdpt == NULL) 
    {
        kernel_panic("Out of memory", NULL);
        return;
    }

    uint64_t* pd = get_or_allocate_table(pdpt, pdpt_idx);
    if (pd == NULL)
    {
        kernel_panic("Out of memory", NULL);
        return;
    }

    // Map the 2MB physical page with supplied flags + HUGE bit
    pd[pd_idx] = (phys_addr & ~(HUGE_PAGE_SIZE - 1)) | flags | PAGE_PRESENT | PAGE_HUGE;

    // Invalidate single page in TLB instead of reloading entire CR3
    __asm__ volatile("invlpg (%0)" :: "r"(virt_addr) : "memory");
}

static void map_page_4k_in(uint64_t* pml4_virt, uint64_t phys_addr, uint64_t virt_addr, uint64_t flags)
{
    uint32_t pml4_idx = PML4_GET_INDEX(virt_addr);
    uint32_t pdpt_idx = PDPT_GET_INDEX(virt_addr);
    uint32_t pd_idx   = PD_GET_INDEX(virt_addr);
    uint32_t pt_idx   = PT_GET_INDEX(virt_addr);

    uint64_t* pdpt = get_or_allocate_table(pml4_virt, pml4_idx);
    if (pdpt == 0)
    {
        kernel_panic("Out of memory", NULL);
        return;
    }

    uint64_t* pd = get_or_allocate_table(pdpt, pdpt_idx);
    if (pd == 0)
    {
        kernel_panic("Out of memory", NULL);
        return;
    }

    uint64_t* pt = get_or_allocate_table(pd, pd_idx);
    if (pt == 0)
    {
        kernel_panic("Out of memory", NULL);
        return;
    }

    // Map the 4KB physical page with supplied flags
    pt[pt_idx] = (phys_addr & PHYS_ADDR_MASK) | flags | PAGE_PRESENT;

    // Invalidate single page in TLB
    __asm__ volatile("invlpg (%0)" :: "r"(virt_addr) : "memory");
}

static uint64_t unmap_page_2mb_in(uint64_t* pml4_virt, uint64_t virt_addr)
{
    uint32_t pml4_idx = PML4_GET_INDEX(virt_addr);
    uint32_t pdpt_idx = PDPT_GET_INDEX(virt_addr);
    uint32_t pd_idx   = PD_GET_INDEX(virt_addr);

    if (!(pml4_virt[pml4_idx] & PAGE_PRESENT)) return 0;

    uint64_t pdpt_phys = pml4_virt[pml4_idx] & PHYS_ADDR_MASK;
    uint64_t* pdpt = (uint64_t*)PHYS_TO_VIRT(pdpt_phys);
    if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) return 0;

    uint64_t pd_phys = pdpt[pdpt_idx] & PHYS_ADDR_MASK;
    uint64_t* pd = (uint64_t*)PHYS_TO_VIRT(pd_phys);
    if (!(pd[pd_idx] & PAGE_PRESENT) || !(pd[pd_idx] & PAGE_HUGE)) return 0;

    uint64_t phys_addr = pd[pd_idx] & PHYS_ADDR_MASK & ~(HUGE_PAGE_SIZE - 1);
    pd[pd_idx] = 0;

    if (reclaim_table(pd, pd_phys, pdpt, pdpt_idx))
    {
        reclaim_table(pdpt, pdpt_phys, pml4_virt, pml4_idx);
    }

    __asm__ volatile("invlpg (%0)" :: "r"(virt_addr) : "memory");

    return phys_addr;
}

static uint64_t unmap_page_4k_in(uint64_t* pml4_virt, uint64_t virt_addr)
{
    uint32_t pml4_idx = PML4_GET_INDEX(virt_addr);
    uint32_t pdpt_idx = PDPT_GET_INDEX(virt_addr);
    uint32_t pd_idx   = PD_GET_INDEX(virt_addr);
    uint32_t pt_idx   = PT_GET_INDEX(virt_addr);

    if (!(pml4_virt[pml4_idx] & PAGE_PRESENT)) return 0;

    uint64_t pdpt_phys = pml4_virt[pml4_idx] & PHYS_ADDR_MASK;
    uint64_t* pdpt = (uint64_t*)PHYS_TO_VIRT(pdpt_phys);
    if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) return 0;

    uint64_t pd_phys = pdpt[pdpt_idx] & PHYS_ADDR_MASK;
    uint64_t* pd = (uint64_t*)PHYS_TO_VIRT(pd_phys);
    if (!(pd[pd_idx] & PAGE_PRESENT)) return 0;

    uint64_t pt_phys;
    uint64_t* pt;

    if (pd[pd_idx] & PAGE_HUGE)
    {
        pt_phys = split_huge_page(pd, pd_idx);

        if (!pt_phys) return 0;
    }
    else
    {
        pt_phys = pd[pd_idx] & PHYS_ADDR_MASK;
    }

    pt = (uint64_t*)PHYS_TO_VIRT(pt_phys);
    if (!(pt[pt_idx] & PAGE_PRESENT)) return 0;

    uint64_t phys_addr = pt[pt_idx] & PHYS_ADDR_MASK;
    pt[pt_idx] = 0;


    if (reclaim_table(pt, pt_phys, pd, pd_idx))
    {
        if (reclaim_table(pd, pd_phys, pdpt, pdpt_idx))
        {
            reclaim_table(pdpt, pdpt_phys, pml4_virt, pml4_idx);
        }
    }

    __asm__ volatile("invlpg (%0)" :: "r"(virt_addr) : "memory");

    return phys_addr;
}

static void vmm_unmap_range_in(uint64_t virt_addr, size_t size, bool free)
{
    if (!IS_ALIGNED(virt_addr, PAGE_SIZE))
    {
        return;
    }

    size = ALIGN_UP(size, PAGE_SIZE);

    uint64_t unmapped = 0;

    while (unmapped < size)
    {
        uint64_t remaining = size - unmapped;
        uint64_t curr_virt = virt_addr + unmapped;

        if (remaining >= HUGE_PAGE_SIZE && IS_ALIGNED(curr_virt, HUGE_PAGE_SIZE))
        {
            uint64_t phys = vmm_unmap_page_2mb(curr_virt);
            if (phys)
            {
                unmapped += HUGE_PAGE_SIZE;

                if (free) pmm_free_range(phys, HUGE_PAGE_SIZE);

                continue;
            }
        }

        uint64_t phys = vmm_unmap_page_4kb(curr_virt);
        if (phys)
        {
            unmapped += PAGE_SIZE;

            if (free) pmm_free_range(phys, PAGE_SIZE);
        }
        else 
        {
            unmapped += PAGE_SIZE;

            continue;
        }
    }
}

static inline uint64_t* current_pml4(void)
{
    uint64_t pml4_phys = read_cr3() & PHYS_ADDR_MASK;
    return (uint64_t*)PHYS_TO_VIRT(pml4_phys);
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

    uint64_t* pml4_virt = current_pml4();

    // The boot direct map already covers the low 4GiB (pd0..pd3), so only map
    // whatever RAM exists above that.
    uint64_t phys_addr = 0x100000000ULL; // 4GiB
    uint64_t virt_addr = DIRECT_MAP_BASE + phys_addr;

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

    // Unmap kernel stack guard page
    vmm_unmap_range((uint64_t)stack_guard, PAGE_SIZE);
}

void vmm_map_page_2mb(uint64_t phys_addr, uint64_t virt_addr, uint64_t flags)
{
    map_page_2mb_in(current_pml4(), phys_addr, virt_addr, flags);
}

void vmm_map_page_4kb(uint64_t phys_addr, uint64_t virt_addr, uint64_t flags)
{
    map_page_4k_in(current_pml4(), phys_addr, virt_addr, flags);
}

uint64_t vmm_unmap_page_2mb(uint64_t virt_addr)
{
    return unmap_page_2mb_in(current_pml4(), virt_addr);
}

uint64_t vmm_unmap_page_4kb(uint64_t virt_addr)
{
    return unmap_page_4k_in(current_pml4(), virt_addr);
}

void vmm_map_range(uint64_t phys_addr, uint64_t virt_addr, size_t size, uint64_t flags)
{
    uint64_t mapped = 0;

    while (mapped < size)
    {
        uint64_t remaining = size - mapped;
        uint64_t curr_phys = phys_addr + mapped;
        uint64_t curr_virt = virt_addr + mapped;

        if (remaining >= HUGE_PAGE_SIZE && IS_ALIGNED(curr_phys, HUGE_PAGE_SIZE) && IS_ALIGNED(curr_virt, HUGE_PAGE_SIZE))
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

void vmm_alloc_map_range(uint64_t virt_addr, size_t size, uint64_t flags)
{
    uint64_t mapped = 0;

    while (mapped < size)
    {
        uint64_t remaining = size - mapped;
        uint64_t curr_virt = virt_addr + mapped;

        if (remaining >= HUGE_PAGE_SIZE && IS_ALIGNED(curr_virt, HUGE_PAGE_SIZE))
        {
            uint64_t phys = pmm_alloc_huge_frame();
            if (phys)
            {
                vmm_map_page_2mb(phys, curr_virt, flags);
                mapped += HUGE_PAGE_SIZE;
                continue;
            }
        }

        uint64_t phys = pmm_alloc_frame();
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

void vmm_free_unmap_range(uint64_t virt_addr, size_t size)
{
    vmm_unmap_range_in(virt_addr, size, true);
}

void vmm_unmap_range(uint64_t virt_addr, size_t size)
{
    vmm_unmap_range_in(virt_addr, size, false);
}