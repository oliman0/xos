bits 64
section .text

global switch_to_thread
global scheduler_yield_asm

extern scheduler_preempt

KERNEL_DATA_SELECTOR equ 0x10
KERNEL_CODE_SELECTOR equ 0x08

; switch_context(uint64_t* new_rsp)
; rdi = new_rsp
switch_to_thread:
    mov rsp, rdi

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    add rsp, 16

    iretq

scheduler_yield_asm:
    pushfq
    pop rax
    cli

    ; Calculate rsp prior to yield call
    mov r11, rsp
    add r11, 8

    ; Construct the iretq frame
    push qword KERNEL_DATA_SELECTOR ; SS
    push r11                        ; RSP
    push rax                        ; RFLAGS
    push qword KERNEL_CODE_SELECTOR ; CS
    lea r11, [rel .return]          ; RIP
    push r11

    ; Construct dummy vector and error code
    push qword 0
    push qword 0

    ; Construct general purpose registers
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, rsp
    call scheduler_preempt

.return:
    sti
    ret