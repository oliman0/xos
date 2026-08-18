bits 64
section .text

global load_gdt
global load_tss

; load_gdt(uint64_t gdt_ptr_addr)
; rdi = gdt_ptr_addr
load_gdt:
    lgdt [rdi]

    ; Reload data segment registers with Kernel Data Selector (0x10)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax

    ; Zero out FS/GS to avoid stale discriptors
    xor ax, ax
    mov fs, ax
    mov gs, ax

    ; Reload code segment register using far return
    push 0x08 ; Push kernel code selector 0x08
    push .flush ; Push the return address
    retfq

.flush:
    ret

; load_tss()
load_tss:
    mov ax, 0x28 ; Tss selector
    ltr ax
    ret