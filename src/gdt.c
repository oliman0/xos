#include <kernel/gdt.h>
#include <kernel/tss.h>

extern void load_gdt(uint64_t gdt_ptr);
extern void load_tss(void);

// Define entries: Null, Kernel Code, Kernel Data, User Code, User Data, TSS (2 slots)
static gdt_entry_t gdt[7] __attribute__((aligned(16)));
static gdt_ptr_t   gdt_p;

void gdt_set_entry(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags) {
    gdt[num].base_low    = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high   = (base >> 24) & 0xFF;

    gdt[num].limit_low   = (limit & 0xFFFF);
    gdt[num].flags       = ((limit >> 16) & 0x0F) | (flags & 0xF0);

    gdt[num].access      = access;
}

void gdt_set_tss(int num, uint64_t base, uint32_t limit) {
    // Fill lower 8 bytes (Standard structure with access 0x89 = Present, Ring 0, TSS Available)
    gdt_set_entry(num, (uint32_t)base, limit, 0x89, 0x00);

    // Fill upper 8 bytes into the adjacent slot
    gdt_tss_entry_t* tss_slot = (gdt_tss_entry_t*)&gdt[num];
    tss_slot->base_upper = (uint32_t)(base >> 32);
    tss_slot->reserved   = 0;
}

void gdt_init() {
    // Null descriptor
    gdt_set_entry(0, 0, 0, 0, 0);

    // Kernel Code
    // Access: Present, Ring 0, Writable (0x9A)
    // Flags: Long mode bit set, 4KB granularity set (0xA0)
    gdt_set_entry(1, 0, 0xFFFFFFFF, 0x9A, 0xA0);

    // Kernel Data
    // Access: Present, Ring 0, Writable (0x92)
    // Flags: Default/ignored in 64-bit, but 4KB granularity set (0x80)
    gdt_set_entry(2, 0, 0xFFFFFFFF, 0x92, 0x80);

    // User Code
    // Access: Present, Ring 3, Executable, Readable (0xFA)
    // Flags: Long mode bit set (0x20)
    gdt_set_entry(3, 0, 0xFFFFFFFF, 0xFA, 0x20);

    // User Data
    // Access: Present, Ring 3, Writable (0xF2)
    // Flags: 4KB granularity set (0x80)
    gdt_set_entry(4, 0, 0xFFFFFFFF, 0xF2, 0x80);

    tss_init();

    // Populate GDT pointer
    gdt_p.limit = sizeof(gdt) - 1;
    gdt_p.base  = (uint64_t)&gdt;

    load_gdt((uint64_t)&gdt_p);

    load_tss();
}