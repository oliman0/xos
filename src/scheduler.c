#include <stddef.h>
#include <kernel/scheduler.h>
#include <kernel/drivers/lapic.h>
#include <kernel/mem/heap.h>
#include <kernel/mem/vmm.h>

static thread_t* current_thread = NULL;
static thread_t* ready_queue_head = NULL;
static thread_t* ready_queue_tail = NULL;
static thread_t* zombie_queue_head = NULL;

static thread_t* idle_thread = NULL;

extern void switch_to_thread(uint64_t* new_rsp);
extern void scheduler_yield_asm();

void idle_thread_func() {
    while (1) {
        __asm__ volatile("hlt");
    }
}

static void clear_dead_threads()
{
    thread_t* curr = zombie_queue_head;
    zombie_queue_head = NULL;

    while (curr != NULL)
    {
        thread_t* next = curr->next;

        if (curr->stack_base)
        {
            vmm_free_unmap_range((uint64_t)curr->stack_base, THREAD_STACK_SIZE);
        }

        kfree(curr);

        curr = next;
    }
}

void scheduler_init()
{
    // Initialize the existing main thread
    thread_t* main_thread = kmalloc(sizeof(thread_t));
    main_thread->id = 0xFFFFFFFF;
    main_thread->state = THREAD_RUNNING;
    main_thread->stack_base = NULL;
    main_thread->next = NULL;
    current_thread = main_thread;

    idle_thread = create_kernel_thread(idle_thread_func);
}

void scheduler_ready(thread_t* thread)
{
    if (!thread || thread == idle_thread) return;

    uint64_t flags;
    __asm__ volatile("pushfq; pop %0; cli" : "=r"(flags) :: "memory");

    thread->state = THREAD_READY;
    thread->next = NULL;

    if (ready_queue_head == NULL)
    {
        ready_queue_head = thread;
        ready_queue_tail = thread;
    }
    else
    {
        ready_queue_tail->next = thread;
        ready_queue_tail = thread;
    }

    if (flags & (1 << 9))
        __asm__ volatile("sti" ::: "memory");
}

void thread_exit()
{
    current_thread->state = THREAD_DEAD;
    scheduler_yield();
}

void scheduler_yield()
{
    scheduler_yield_asm();
}

void scheduler_preempt(registers_t* regs)
{
    __asm__ volatile("cli");

    clear_dead_threads();

    if (current_thread->state == THREAD_RUNNING)
    {
        current_thread->rsp = (uint64_t*)regs;
        if (current_thread != idle_thread)
        {
            scheduler_ready(current_thread);
        }
    }
    else if (current_thread->state == THREAD_DEAD)
    {
        current_thread->next = zombie_queue_head;
        zombie_queue_head = current_thread;
    }

    thread_t* next_thread = NULL;
    if (ready_queue_head != NULL)
    {
        next_thread = ready_queue_head;
        ready_queue_head = next_thread->next;
        if (ready_queue_head == NULL)
        {
            ready_queue_tail = NULL;
        }
        next_thread->next = NULL;
    }
    else
    {
        next_thread = idle_thread;
    }

    next_thread->state = THREAD_RUNNING;
    current_thread = next_thread;

    switch_to_thread(current_thread->rsp);
}