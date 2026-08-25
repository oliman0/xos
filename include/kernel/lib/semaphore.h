#ifndef XOS_SEMAPHORE_H
#define XOS_SEMAPHORE_H

#include <stdint.h>
#include <stdbool.h>

#include <kernel/thread.h>

typedef struct {
    uint32_t count;
    uint32_t max_count;
    wait_queue_node_t* wait_queue_head;
    wait_queue_node_t* wait_queue_tail;
} semaphore_t;

semaphore_t* semaphore_create(uint32_t initial_count, uint32_t max_count);

void semaphore_free(semaphore_t* sem);

bool semaphore_wait(semaphore_t* sem, uint32_t count, uint32_t timeout_ms);

bool semaphore_signal(semaphore_t* sem, uint32_t count);

#endif