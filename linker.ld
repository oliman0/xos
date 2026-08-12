ENTRY(_start)

KERNEL_VMA = 0xFFFFFFFF80000000;
KERNEL_PMA = 0x0000000000100000;

SECTIONS
{
    . = KERNEL_PMA;

    .boot : ALIGN(4K)
    {
        KEEP(*(.multiboot*))
        *(.boot)
    }

    . = KERNEL_VMA + .;

    /* Load this at physical LMA, but compiler expects it at VMA */
    .text ALIGN(4K) : AT(ADDR(.text) - KERNEL_VMA)
    {
        *(.text)
    }

    .rodata ALIGN(4K) : AT(ADDR(.rodata) - KERNEL_VMA)
    {
        *(.rodata)
    }

    .data ALIGN(4K) : AT(ADDR(.data) - KERNEL_VMA)
    {
        *(.data)
    }

    .setup_stack ALIGN(16) : AT(ADDR(.setup_stack) - KERNEL_VMA) {
        *(.setup_stack)
    }

    .bss ALIGN(4K) : AT(ADDR(.bss) - KERNEL_VMA)
    {
        _bss_start = .;
        *(.bss)
        *(COMMON)
        _bss_end = .;

        _kernel_end = .;
    }
}