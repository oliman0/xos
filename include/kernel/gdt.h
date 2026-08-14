#ifndef XOS_GDT_H
#define XOS_GDT_H

#include <stdint.h>

// 8-byte GDT entry
typedef struct __attribute__((packed))
{
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  flags;
    uint8_t  base_high;
} gdt_entry_t;

// 16-byte TSS Entry
typedef struct __attribute__((packed))
{
    gdt_entry_t lower;
    uint32_t    base_upper;
    uint32_t    reserved;
} gdt_tss_entry_t;

typedef struct __attribute__((packed))
{
    uint16_t limit;
    uint64_t base;
} gdt_ptr_t;

void gdt_set_entry(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags);
void gdt_set_tss(int num, uint64_t base, uint32_t limit);

void gdt_init();

#endif