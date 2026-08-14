#include <kernel/tss.h>
#include <kernel/gdt.h>
#include <kernel/lib/string.h>

static tss_entry_t tss __attribute__((aligned(16)));

static uint8_t double_fault_stack[DOUBLE_FAULT_STACK_SIZE] __attribute__((aligned(16)));

void tss_init()
{
    memset(&tss, 0, sizeof(tss));

    // Stack for Ring 3 -> Ring 0
    tss.rsp0 = (uint64_t)stack_top;

    // Dedicated IST1 Stack for #DF
    uint64_t ist1_top = (uint64_t)&double_fault_stack[DOUBLE_FAULT_STACK_SIZE];
    tss.ist1 = ist1_top;

    // Disable I/O permission bitmap
    tss.iomap_base = sizeof(tss);

    // Register TSS entry in GDT
    gdt_set_tss(5, (uint64_t)&tss, sizeof(tss) - 1);
}

void tss_set_rsp0(uint64_t rsp0)
{
    tss.rsp0 = rsp0;
}