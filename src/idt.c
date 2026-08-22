#include <stddef.h>
#include <kernel/idt.h>
#include <kernel/kernel_io.h>
#include <kernel/panic.h>
#include <kernel/drivers/lapic.h>
#include <kernel/lib/string.h>

static idt_entry_t idt[IDT_ENTRIES] __attribute__((aligned(16)));
static idtr_t      idtr;

extern isr_stub_t* isr_stub_table[IDT_ENTRIES];

static void idt_set_entry(uint8_t num, uint64_t isr_stub, uint16_t selector, uint8_t flags, uint8_t ist)
{
    idt[num].offset_low      = (uint16_t)(isr_stub & 0xFFFF);
    idt[num].selector        = selector;
    idt[num].ist             = ist & 0x07;
    idt[num].type_attributes = flags;
    idt[num].offset_mid      = (uint16_t)((isr_stub >> 16) & 0xFFFF);
    idt[num].offset_high     = (uint32_t)((isr_stub >> 32) & 0xFFFFFFFF);
    idt[num].zero            = 0;
}

static irq_handler_t irq_handlers[IDT_ENTRIES];

void idt_register_interrupt_handler(uint8_t vector, irq_handler_t handler)
{
    irq_handlers[vector] = handler;
}

void idt_init()
{
    idtr.limit = sizeof(idt) - 1;
    idtr.base  = (uint64_t)&idt;

    for (int i = 0; i < IDT_ENTRIES; i++)
    {
        uint8_t flags = IDT_ENTRY_PRESENT | IDT_ENTRY_RING_0 | IDT_ENTRY_INTERRUPT_64GATE;
        uint8_t ist   = 0;

        if (i == IDT_VECTOR_DF)
        {
            ist = 1;
        }

        idt_set_entry(i, (uint64_t)isr_stub_table[i], IDT_KERNEL_CS, flags, ist);
    }

    memset(irq_handlers, 0, sizeof(irq_handlers));

    // Load IDTR via inline assembly
    __asm__ volatile ("lidt %0" : : "m"(idtr));
}

static const char* exception_messages[IDT_CPU_EXCEPTION_COUNT] = {
    "Divide-by-Zero Exception (#DE)",
    "Debug Exception (#DB)",
    "Non-Maskable Interrupt (#NMI)",
    "Breakpoint Exception (#BP)",
    "Overflow Exception (#OF)",
    "Bound Range Exceeded (#BR)",
    "Invalid Opcode (#UD)",
    "Device Not Available (#NM)",
    "Double Fault (#DF)",
    "Coprocessor Segment Overrun",
    "Invalid TSS (#TS)",
    "Segment Not Present (#NP)",
    "Stack-Segment Fault (#SS)",
    "General Protection Fault (#GP)",
    "Page Fault (#PF)",
    "Reserved Exception (15)",
    "x87 Floating-Point Exception (#MF)",
    "Alignment Check Exception (#AC)",
    "Machine Check Exception (#MC)",
    "SIMD Floating-Point Exception (#XM/#XF)",
    "Virtualization Exception (#VE)",
    "Control Protection Exception (#CP)",
    "Reserved (22)", "Reserved (23)", "Reserved (24)",
    "Reserved (25)", "Reserved (26)", "Reserved (27)", "Reserved (28)",
    "VMM Communication Exception (#VC)",
    "Security Exception (#SX)",
    "Reserved (31)"
};

void isr_handler(registers_t* regs)
{
    if (regs->vector >= IDT_CPU_EXCEPTION_COUNT && regs->vector < IDT_ENTRIES - 1)
    {
        if (irq_handlers[regs->vector] != NULL)
        {
            irq_handlers[regs->vector](regs);
        }
        else
        {
            kprintf("Unhandled interrupt %d\n", regs->vector);
        }

        lapic_eoi();
        return;
    }

    if (regs->vector < IDT_CPU_EXCEPTION_COUNT)
    {
        kernel_panic(exception_messages[regs->vector], regs);
    }
}