#ifndef XOS_PANIC_H
#define XOS_PANIC_H

#include <stdint.h>
#include <kernel/idt.h>

void kernel_panic(const char* message, registers_t* regs);

#define KPANIC(msg) kernel_panic(msg, NULL)
#define KASSERT(cond, msg) if (!(cond)) { kernel_panic("Assertion Failed: " msg, NULL); }

#endif