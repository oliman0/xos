global _start
extern kernel_main

KERNEL_VMA equ 0xFFFFFFFF80000000

section .multiboot
align 8
mb2_header_start:
    dd 0xe85250d6                        ; MB2 Magic Number
    dd 0                                 ; Architecture: 32-bit Protected Mode
    dd mb2_header_end - mb2_header_start ; Header length
    dd 0x100000000 - (0xe85250d6 + 0 + (mb2_header_end - mb2_header_start)) ; Checksum

align 8
framebuffer_tag_start:
    dw 5                                 ; Type: Framebuffer
    dw 0                                 ; Flags
    dd framebuffer_tag_end - framebuffer_tag_start
    dd 0                                 ; Width (0 = no preference)
    dd 0                                 ; Height (0 = no preference)
    dd 0                                 ; Depth (0 = no preference)
framebuffer_tag_end:

align 8
end_tag_start:
    dw 0                                 ; End tag
    dw 0
    dd 8
mb2_header_end:

section .boot
bits 32
_start:
    ; Verify Multiboot2 Magic Number
    cmp eax, 0x36d76289
    jne .multiboot_error

    mov esp, stack_top - KERNEL_VMA

    mov dword [mb_info_ptr - KERNEL_VMA], ebx     ; Save Multiboot2 info struct pointer

    call setup_page_tables
    call enable_paging

    lgdt [gdt64_phys_pointer - KERNEL_VMA]
    jmp gdt64.code:(long_mode_start - KERNEL_VMA)

.multiboot_error:
    cli
    hlt
    jmp .multiboot_error

setup_page_tables:
    ; Identity Map PML4[0] -> PDPT
    mov eax, pdpt - KERNEL_VMA
    or eax, 0x3                 ; Present + Writable
    mov [pml4 - KERNEL_VMA], eax

    ; Identity Map PDPT[0..3] -> pd0..pd3 (4GB)
    mov eax, pd0 - KERNEL_VMA
    or eax, 0x3
    mov [pdpt - KERNEL_VMA + 0], eax

    mov eax, pd1 - KERNEL_VMA
    or eax, 0x3
    mov [pdpt - KERNEL_VMA + 8], eax

    mov eax, pd2 - KERNEL_VMA
    or eax, 0x3
    mov [pdpt - KERNEL_VMA + 16], eax

    mov eax, pd3 - KERNEL_VMA
    or eax, 0x3
    mov [pdpt - KERNEL_VMA + 24], eax

    ; Higher-Half Map PML4[511] -> PDPT_HIGH
    mov eax, pdpt_high - KERNEL_VMA
    or eax, 0x3                 ; Present + Writable
    mov [pml4 - KERNEL_VMA + 511 * 8], eax

    ; Map PDPT_HIGH[510] -> pd0, [511] -> pd1  (covers KERNEL_VMA .. +2GB)
    mov eax, pd0 - KERNEL_VMA
    or eax, 0x3
    mov [pdpt_high - KERNEL_VMA + 510 * 8], eax
    mov eax, pd1 - KERNEL_VMA
    or eax, 0x3
    mov [pdpt_high - KERNEL_VMA + 511 * 8], eax

    ; Direct Map PML4[256] -> PDPT_DM
    mov eax, pdpt_dm - KERNEL_VMA
    or eax, 0x3
    mov [pml4 - KERNEL_VMA + 256 * 8], eax

    ; Map PDPT_DM[0..3] -> pd0..pd3  (direct map covers phys 0 .. 4GB)
    mov eax, pd0 - KERNEL_VMA
    or eax, 0x3
    mov [pdpt_dm - KERNEL_VMA + 0 * 8], eax
    mov eax, pd1 - KERNEL_VMA
    or eax, 0x3
    mov [pdpt_dm - KERNEL_VMA + 1 * 8], eax
    mov eax, pd2 - KERNEL_VMA
    or eax, 0x3
    mov [pdpt_dm - KERNEL_VMA + 2 * 8], eax
    mov eax, pd3 - KERNEL_VMA
    or eax, 0x3
    mov [pdpt_dm - KERNEL_VMA + 3 * 8], eax

    ; Map 4GB using 2048 x 2MB Huge Pages
    ; pd0..pd3 are order contiguously in .data
    ; so map overflows from pd0 -> pd1..pd3
    mov ecx, 0
.map_pd:
    mov eax, 0x200000
    mul ecx                      ; EAX = Low 32 bits, EDX = High 32 bits
    or eax, 0b10000011           ; Present + Writable + Huge Page (2MB)

    mov [pd0 - KERNEL_VMA + ecx * 8], eax     ; Store low 32 bits
    mov [pd0 - KERNEL_VMA + ecx * 8 + 4], edx ; Store high 32 bits (clears upper bits)

    inc ecx
    cmp ecx, 2048                ; 2048 * 2MB = 4GB
    jne .map_pd
    ret

enable_paging:
    mov eax, pml4 - KERNEL_VMA
    mov cr3, eax

    mov eax, cr4
    or eax, 1 << 5               ; Enable PAE
    mov cr4, eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8               ; Enable Long Mode
    wrmsr

    mov eax, cr0
    or eax, 1 << 31               ; Enable Paging
    mov cr0, eax
    ret

section .text
bits 64
long_mode_start:
    ; Zero out data segment registers
    mov ax, 0
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Reload GDT safely using higher-half virtual address
    lgdt [rel gdt64_virt_pointer]

    ; Set RSP to higher-half stack
    mov rsp, stack_top
    and rsp, -16

    ; Load saved Multiboot pointer into RDI (1st argument in System V ABI)
    mov edi, dword [rel mb_info_ptr]

    ; Add higher half kernel offset to the Multiboot pointer
    mov rax, KERNEL_VMA
    add rdi, rax

    mov rax, kernel_main
    call rax

    cli
.hang:
    hlt
    jmp .hang

global dbg_play_beep
dbg_play_beep:
    ; Set PIT Channel 2 to square wave mode
    mov al, 0xB6
    out 0x43, al

    ; Load frequency divisor (1193182 Hz / 440 Hz ≈ 2711)
    mov ax, 2711
    out 0x42, al        ; Low byte
    mov al, ah
    out 0x42, al        ; High byte

    ; Turn speaker ON (Bits 0 and 1 of port 0x61)
    in al, 0x61
    or al, 0x03
    out 0x61, al
    ret

section .setup_stack nobits
align 16
stack_bottom: resb 16384
global stack_top
stack_top:

section .bss

section .rodata
align 8
gdt64:
    dq 0
.code: equ $ - gdt64
    dq (1<<43) | (1<<44) | (1<<47) | (1<<53)
.data: equ $ - gdt64
    dq (1<<41) | (1<<44) | (1<<47)
gdt64_end:

align 4
gdt64_phys_pointer:
    dw gdt64_end - gdt64 - 1
    dd gdt64 - KERNEL_VMA   ; 32-bit physical address offset

align 4
gdt64_virt_pointer:
    dw gdt64_end - gdt64 - 1
    dq gdt64                ; 64-bit virtual address offset

section .data
align 4096
global pml4
pml4:      times 4096 db 0
pdpt:      times 4096 db 0
pdpt_high: times 4096 db 0
pdpt_dm:   times 4096 db 0
pd0:       times 4096 db 0
pd1:       times 4096 db 0
pd2:       times 4096 db 0
pd3:       times 4096 db 0

align 8
mb_info_ptr: dq 0           ; Stores 32-bit Multiboot pointer