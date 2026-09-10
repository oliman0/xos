#include <kernel/idt.h>
#include <kernel/kernel_io.h>
#include <kernel/scheduler.h>
#include <kernel/thread.h>
#include <kernel/drivers/linear_framebuffer.h>
#include <kernel/lib/string.h>
#include <kernel/mem/heap.h>
#include <kernel/mem/pmm.h>
#include <kernel/mem/vmm.h>

static uint64_t next_thread_id = 0;
static uint64_t next_stack_addr = THREAD_STACK_BASE;

void thread_wrapper(void (*entry_point)()) {
    if (entry_point)
    {
        entry_point();
    }

    thread_exit();
}

thread_t* create_kernel_thread(void (*entry_point)())
{
    thread_t* thread = kmalloc(sizeof(thread_t));
    if (!thread) return NULL;

    // Unmap the guard page if previously mapped
    // Otherwise unmap just returns
    vmm_free_unmap_range(next_stack_addr, THREAD_STACK_GUARD_SIZE);

    // Allocate, map and initialize the stack
    uint64_t stack_base = next_stack_addr + THREAD_STACK_GUARD_SIZE;
    next_stack_addr += THREAD_STACK_SIZE + THREAD_STACK_GUARD_SIZE;
    vmm_alloc_map_range(stack_base, THREAD_STACK_SIZE, PAGE_WRITABLE | PAGE_NX);
    memset((void*)stack_base, 0, THREAD_STACK_SIZE);

    // Prepare the register frame at the top of the stack
    uint64_t stack_top = stack_base + THREAD_STACK_SIZE;
    registers_t* regs = (registers_t*)(stack_top - sizeof(registers_t));

    // Set up the hardware-pushed context
    regs->rip = (uint64_t)thread_wrapper;
    regs->cs = IDT_KERNEL_CS;
    regs->rflags = 0x202;      // IF (Interrupt Flag) enabled + Reserved bit
    regs->rsp = stack_top - 8;
    regs->ss = IDT_KERNEL_DS;

    // Set up the general-purpose registers
    // Pass the entry_point as the first argument to thread_wrapper (RDI)
    regs->rdi = (uint64_t)entry_point;
    regs->rbp = 0;

    // Initialize thread structure
    thread->rsp = (uint64_t*)regs;
    thread->id = next_thread_id++;
    thread->state = THREAD_DEAD;
    thread->stack_base = (void*)stack_base;
    thread->next = NULL;

    thread->wait_queue_node = kmalloc(sizeof(wait_queue_node_t));
    if (!thread->wait_queue_node) {
        kfree(thread);

        vmm_free_unmap_range(stack_base, THREAD_STACK_SIZE);
        next_stack_addr -= THREAD_STACK_SIZE + THREAD_STACK_GUARD_SIZE;

        return NULL;
    }

    thread->wait_queue_node->thread = thread;
    thread->wait_queue_node->next = NULL;
    thread->wait_queue_node->state = NODE_WAITING;
    thread->wait_queue_node->requested_count = 0;

    return thread;
}

void free_kernel_thread(thread_t* thread)
{
    if (!thread) return;

    // Free the stack
    if (thread->stack_base) {
        vmm_free_unmap_range((uint64_t)thread->stack_base, THREAD_STACK_SIZE);
    }

    // Free the wait queue node
    if (thread->wait_queue_node) {
        kfree(thread->wait_queue_node);
    }

    // Free the thread structure
    kfree(thread);
}