#ifndef XOS_THREAD_H
#define XOS_THREAD_H

#include <stdint.h>

#define THREAD_STACK_BASE 0xFFFFB00000000000ULL
#define THREAD_STACK_SIZE 0x4000

typedef enum {
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_DEAD
} thread_state_t;

typedef struct thread {
    uint64_t* rsp;            // Saved stack pointer (must be first for easy asm access)
    uint64_t id;
    thread_state_t state;
    void* stack_base;         // For freeing the stack later
    struct thread* next;      // Simple linked list for the scheduler
} thread_t;

thread_t* create_kernel_thread(void (*entry_point)());

#endif