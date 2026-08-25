#include <kernel/thread.h>
#include <kernel/lib/semaphore.h>
#include <kernel/mem/heap.h>
#include <kernel/scheduler.h>
#include <kernel/arch/intr.h>
#include <kernel/drivers/lapic.h>

static void wait_queue_remove(semaphore_t* sem, wait_queue_node_t* node)
{
    if (!sem->wait_queue_head || !node) return;

    if (sem->wait_queue_head == node) {
        sem->wait_queue_head = node->next;
        if (sem->wait_queue_head == NULL) {
            sem->wait_queue_tail = NULL;
        }
        node->next = NULL;
        return;
    }

    wait_queue_node_t* curr = sem->wait_queue_head;
    while (curr->next != NULL) {
        if (curr->next == node) {
            curr->next = node->next;
            if (sem->wait_queue_tail == node) {
                sem->wait_queue_tail = curr;
            }
            node->next = NULL;
            break;
        }
        curr = curr->next;
    }
}

semaphore_t* semaphore_create(uint32_t initial_count, uint32_t max_count) 
{
    if (max_count == 0 || initial_count > max_count) 
    {
        return NULL;
    }

    semaphore_t* sem = (semaphore_t*)kmalloc(sizeof(semaphore_t));
    if (!sem) {
        return NULL;
    }

    sem->count = initial_count;
    sem->max_count = max_count;
    sem->wait_queue_head = NULL;
    sem->wait_queue_tail = NULL;

    return sem;
}

void semaphore_free(semaphore_t* sem) 
{
    if (sem == NULL) return;

    uint64_t flags;
    disable_interrupts(&flags);

    // Unblock all remaining threads to prevent dangling queue pointers
    while (sem->wait_queue_head != NULL) {
        wait_queue_node_t* node = sem->wait_queue_head;
        sem->wait_queue_head = node->next;
        node->next = NULL;
        node->state = NODE_ABORTED;
        scheduler_ready(node->thread);
    }

    sem->wait_queue_tail = NULL;

    enable_interrupts(flags);

    kfree(sem);
}

bool semaphore_wait(semaphore_t* sem, uint32_t count, uint32_t timeout_ms)
{
    if (sem == NULL || count == 0 || count > sem->max_count) return false;

    uint64_t flags;
    disable_interrupts(&flags);

    /////////////////////
    // Fast path
    if (sem->count >= count && sem->wait_queue_head == NULL) {
        sem->count -= count;
        enable_interrupts(flags);
        return true;
    }

    if (timeout_ms == 0) {
        enable_interrupts(flags);
        return false;
    }
    //////////////////////

    bool is_infinite = (timeout_ms == UINT32_MAX);
    uint64_t start_tick = lapic_get_kernel_ticks();
    uint64_t timeout_tick = 0;

    if (!is_infinite) {
        timeout_tick = start_tick + lapic_ms_to_ticks(timeout_ms);
    }

    // Enqueue current thread
    wait_queue_node_t* curr = scheduler_current_thread()->wait_queue_node;
    curr->next = NULL;
    curr->state = NODE_WAITING;
    curr->requested_count = count;

    if (sem->wait_queue_head == NULL) {
        sem->wait_queue_head = curr;
        sem->wait_queue_tail = curr;
    } else {
        sem->wait_queue_tail->next = curr;
        sem->wait_queue_tail = curr;
    }

    while (sem->count < count || sem->wait_queue_head != curr) 
    {
        if (curr->state == NODE_ABORTED)
        {
            enable_interrupts(flags);
            return false;
        }

        uint64_t current_tick = lapic_get_kernel_ticks();
        if (!is_infinite && (int64_t)(current_tick - timeout_tick) >= 0) {
            wait_queue_remove(sem, curr);
            enable_interrupts(flags);
            return false;
        }

        curr->thread->state = THREAD_BLOCKED;
        scheduler_yield(); // Interrupts restored for next thread
        disable_interrupts(&flags);
    } 

    wait_queue_remove(sem, curr);
    sem->count -= count;

    enable_interrupts(flags);
    return true;
}

bool semaphore_signal(semaphore_t* sem, uint32_t count)
{
    if (sem == NULL || count == 0) return false;

    uint64_t flags;
    disable_interrupts(&flags);

    if (sem->count > sem->max_count ||
    count > sem->max_count - sem->count)
    {
        enable_interrupts(flags);
        return false;
    }

    sem->count += count;

    uint32_t available = sem->count;
    wait_queue_node_t* node = sem->wait_queue_head;

    while (node != NULL && available >= node->requested_count)
    {
        available -= node->requested_count;
        
        if (node->state == NODE_WAITING)
        {
            node->state = NODE_WOKEN;
            scheduler_ready(node->thread);
        }

        node = node->next;
    }

    enable_interrupts(flags);
    return true;
}