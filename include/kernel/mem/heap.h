#ifndef XOS_HEAP_H
#define XOS_HEAP_H

#include <stdbool.h>
#include <stddef.h>

#define KERNEL_HEAP_BASE 0xFFFFA00000000000ULL
#define KERNEL_HEAP_SIZE 0x40000000ULL
#define KERNEL_HEAP_EXPAND_SIZE 0x4000ULL

typedef struct __attribute__((aligned(16))) heap_block {
    size_t size;
    struct heap_block* next;
    struct heap_block* prev;
    bool is_free;
} heap_block_t;

void heap_init();

void* kmalloc(size_t size);

void kfree(void* ptr);

#endif