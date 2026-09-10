#ifndef XOS_THREAD_H
#define XOS_THREAD_H

#include <stdint.h>
#include <stdbool.h>

#include <kernel/mem/pmm.h>

#define THREAD_STACK_BASE 0xFFFFB00000000000ULL
#define THREAD_STACK_SIZE 0x4000
#define THREAD_STACK_GUARD_SIZE PAGE_SIZE

typedef enum {
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_DEAD
} thread_state_t;

typedef struct wait_queue_node wait_queue_node_t;

typedef struct thread {
    uint64_t* rsp;            // Saved stack pointer (must be first for easy asm access)
    uint64_t id;
    thread_state_t state;
    void* stack_base;         // For freeing the stack later
    struct thread* next;      // Simple linked list for the scheduler
    wait_queue_node_t* wait_queue_node;
} thread_t;

typedef enum {
    NODE_WAITING,
    NODE_WOKEN,
    NODE_ABORTED
} node_state_t;

typedef struct wait_queue_node {
    thread_t* thread;
    struct wait_queue_node* next;
    node_state_t state;
    uint32_t requested_count;
} wait_queue_node_t;

thread_t* create_kernel_thread(void (*entry_point)());

void free_kernel_thread(thread_t* thread);

#endif