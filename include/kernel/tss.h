#ifndef XOS_TSS_H
#define XOS_TSS_H

#include <stdint.h>

#define DOUBLE_FAULT_STACK_SIZE 8192 // 8 KiB dedicated stack

extern uint8_t stack_top[];

typedef struct __attribute__((packed)) {
    uint32_t reserved0;
    uint64_t rsp0;       // Stack pointer loaded on Ring 3 -> Ring 0 transition
    uint64_t rsp1;       // Unused in 64-bit mode
    uint64_t rsp2;       // Unused in 64-bit mode
    uint64_t reserved1;
    uint64_t ist1;       // Interrupt Stack Table 1 (e.g. Double Fault)
    uint64_t ist2;       // IST 2
    uint64_t ist3;       // IST 3
    uint64_t ist4;       // IST 4
    uint64_t ist5;       // IST 5
    uint64_t ist6;       // IST 6
    uint64_t ist7;       // IST 7
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base; // Offset to I/O Permission Bitmap (sizeof(tss_entry_t) disables it)
} tss_entry_t;

void tss_init();
void tss_set_rsp0(uint64_t rsp0);

#endif