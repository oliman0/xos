#ifndef XOS_SCHEDULER_H
#define XOS_SCHEDULER_H

#include <kernel/thread.h>
#include <kernel/idt.h>

void scheduler_init();

void scheduler_ready(thread_t* thread);

void thread_exit();
void scheduler_yield();

void scheduler_preempt(registers_t* regs);

#endif