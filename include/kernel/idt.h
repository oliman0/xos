#ifndef XOS_IDT_H
#define XOS_IDT_H

#include <stdint.h>

typedef struct __attribute__((packed))
{
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attributes;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;             // Reserved (must be 0)
} idt_entry_t;

typedef struct __attribute__((packed))
{
    uint16_t limit;
    uint64_t base;
} idtr_t;

// Register frame layout matching isr_common_stub pushes
typedef struct __attribute__((packed))
{
    // Pushed by isr_common_stub
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;

    // Pushed by macro stub
    uint64_t vector;
    uint64_t error_code;

    // Pushed automatically by CPU
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} registers_t;

typedef void (*irq_handler_t)(registers_t* regs);

void idt_init();
void idt_register_interrupt_handler(uint8_t vector, irq_handler_t handler);

#endif