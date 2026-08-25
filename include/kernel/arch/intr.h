#ifndef XOS_INTR_H
#define XOS_INTR_H

#include <stdint.h>

static inline void disable_interrupts(uint64_t* flags)
{
    __asm__ volatile("pushfq; pop %0; cli" : "=r"(*flags) :: "memory");
}

static inline void enable_interrupts(uint64_t flags)
{
    if (flags & (1 << 9)) {
        __asm__ volatile("sti" ::: "memory");
    }
}

#endif