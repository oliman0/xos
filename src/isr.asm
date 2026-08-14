bits 64
section .text

extern isr_handler

; Macro for interrupts WITHOUT an error code pushed by CPU
%macro ISR_NOERRCODE 1
global isr_stub_%1
isr_stub_%1:
    push qword 0          ; Push dummy error code
    push qword %1         ; Push interrupt vector number
    jmp isr_common_stub
%endmacro

; Macro for interrupts WITH an error code pushed by CPU
%macro ISR_ERRCODE 1
global isr_stub_%1
isr_stub_%1:
    ; CPU already pushed error code!
    push qword %1         ; Push interrupt vector number
    jmp isr_common_stub
%endmacro

; Generate Stubs 0 through 31 (Exceptions)
ISR_NOERRCODE 0   ; Divide-by-zero
ISR_NOERRCODE 1   ; Debug
ISR_NOERRCODE 2   ; Non-maskable Interrupt
ISR_NOERRCODE 3   ; Breakpoint
ISR_NOERRCODE 4   ; Overflow
ISR_NOERRCODE 5   ; Bound Range Exceeded
ISR_NOERRCODE 6   ; Invalid Opcode
ISR_NOERRCODE 7   ; Device Not Available
ISR_ERRCODE   8   ; Double Fault
ISR_NOERRCODE 9   ; Coprocessor Segment Overrun
ISR_ERRCODE   10  ; Invalid TSS
ISR_ERRCODE   11  ; Segment Not Present
ISR_ERRCODE   12  ; Stack-Segment Fault
ISR_ERRCODE   13  ; General Protection Fault
ISR_ERRCODE   14  ; Page Fault
ISR_NOERRCODE 15  ; Reserved
ISR_NOERRCODE 16  ; x87 Floating-Point Exception
ISR_ERRCODE   17  ; Alignment Check
ISR_NOERRCODE 18  ; Machine Check
ISR_NOERRCODE 19  ; SIMD Floating-Point Exception
ISR_NOERRCODE 20  ; Virtualization Exception
ISR_ERRCODE   21  ; Control Protection Exception
; Vectors 22-28 Reserved
%assign i 22
%rep 7
    ISR_NOERRCODE i
    %assign i i+1
%endrep
ISR_ERRCODE   29  ; VMM Communication Exception
ISR_ERRCODE   30  ; Security Exception
ISR_NOERRCODE 31  ; Reserved

; Generate Stubs 32 through 255 (IRQs & User Interrupts)
%assign i 32
%rep 224
    ISR_NOERRCODE i
    %assign i i+1
%endrep

; Common entry point for all ISRs
isr_common_stub:
    ; Save general-purpose registers
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

    ; Pass pointer to registers_t struct as 1st argument (RDI per System V AMD64 ABI)
    mov rdi, rsp
    call isr_handler

    ; Restore general-purpose registers
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

    ; Clean up error code and interrupt vector number (2 * 8 bytes = 16)
    add rsp, 16

    ; Return from interrupt in 64-bit mode
    iretq

; Generate an array of entry point addresses for C initialization
section .rodata
global isr_stub_table
isr_stub_table:
%assign i 0
%rep 256
    dq isr_stub_%[i]
    %assign i i+1
%endrep