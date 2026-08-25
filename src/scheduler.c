#include <stddef.h>
#include <kernel/scheduler.h>
#include <kernel/drivers/lapic.h>
#include <kernel/mem/heap.h>
#include <kernel/mem/vmm.h>
#include <kernel/arch/intr.h>

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

        free_kernel_thread(curr);

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

    main_thread->wait_queue_node = kmalloc(sizeof(wait_queue_node_t));
    if (main_thread->wait_queue_node == NULL) {
        kfree(main_thread);
        return;
    }

    main_thread->wait_queue_node->thread = main_thread;
    main_thread->wait_queue_node->next = NULL;
    main_thread->wait_queue_node->state = NODE_WAITING;
    main_thread->wait_queue_node->requested_count = 0;

    current_thread = main_thread;

    idle_thread = create_kernel_thread(idle_thread_func);
}

void scheduler_ready(thread_t* thread)
{
    if (thread == NULL || thread == idle_thread ||
        thread->state == THREAD_READY) return;

    uint64_t flags;
    disable_interrupts(&flags);

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

    enable_interrupts(flags);
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

    current_thread->rsp = (uint64_t*)regs;

    if (current_thread->state == THREAD_RUNNING)
    {
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

thread_t* scheduler_current_thread()
{
    return current_thread;
}