#include <kernel/panic.h>
#include <kernel/kernel_io.h>
#include <kernel/drivers/uefi_linear_framebuffer.h>

static inline uint64_t read_cr0(void)
{
    uint64_t val;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(val));
    return val;
}

static inline uint64_t read_cr2(void)
{
    uint64_t val;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(val));
    return val;
}

static inline uint64_t read_cr3(void)
{
    uint64_t val;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(val));
    return val;
}

void kernel_panic(const char* message, registers_t* regs)
{
    fb_clear(0);
    fb_set_front_color(0xFF6B6B);

    // Immediately disable interrupts to prevent secondary faults from interrupting the dump
    __asm__ volatile ("cli");

    kprintf("\n============================================================\n");
    kprintf("                     KERNEL PANIC                           \n");
    kprintf("============================================================\n");
    kprintf("Reason: %s\n\n", message ? message : "Unspecified Fault");

    if (regs)
    {
        kprintf("--- Execution Frame ---\n");
        kprintf("Vector    : %x (%d)\n", regs->vector, regs->vector);
        kprintf("Error Code: %x\n", regs->error_code);
        kprintf("RIP       : %x    CS: %x\n", regs->rip, regs->cs);
        kprintf("RFLAGS    : %x    SS: %x\n", regs->rflags, regs->ss);
        kprintf("RSP       : %x\n\n", regs->rsp);

        kprintf("--- General Purpose Registers ---\n");
        kprintf("RAX: %x  RBX: %x  RCX: %x\n", regs->rax, regs->rbx, regs->rcx);
        kprintf("RDX: %x  RSI: %x  RDI: %x\n", regs->rdx, regs->rsi, regs->rdi);
        kprintf("RBP: %x  R8 : %x  R9 : %x\n", regs->rbp, regs->r8,  regs->r9);
        kprintf("R10: %x  R11: %x  R12: %x\n", regs->r10, regs->r11, regs->r12);
        kprintf("R13: %x  R14: %x  R15: %x\n\n", regs->r13, regs->r14, regs->r15);
    }

    uint64_t cr0 = read_cr0();
    uint64_t cr2 = read_cr2();
    uint64_t cr3 = read_cr3();

    kprintf("--- Control Registers ---\n");
    kprintf("CR0: %x  CR2: %x  CR3: %x\n", cr0, cr2, cr3);
    kprintf("============================================================\n");

    while (1)
    {
        __asm__ volatile ("hlt");
    }
}