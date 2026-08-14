cmake_minimum_required(VERSION 3.20)
project(xos C ASM_NASM)

set(CMAKE_C_STANDARD 11)

set(CMAKE_C_FLAGS "-m64 -mcmodel=kernel -ffreestanding -fno-builtin -fno-stack-protector -mno-red-zone -fno-pie -Wall -Wextra")
set(CMAKE_ASM_NASM_FLAGS "-f elf64")

include_directories(include)

# Define executable target
add_executable(kernel.bin src/boot/boot.asm src/kernel.c
        src/kernel_io.c
        src/lib/font.c
        src/drivers/uefi_linear_framebuffer.c
        src/boot/multiboot2.c
        src/mem/pmm.c
        src/lib/string.c
        src/mem/vmm.c
        src/gdt.c
        src/gdt.asm
        src/tss.c
        src/idt.c
        src/isr.asm
        src/panic.c
        src/drivers/pic.c
        src/drivers/lapic.c
        src/drivers/acpi.c
)

# Force CMake to disable position-independent code
set_property(TARGET kernel.bin PROPERTY POSITION_INDEPENDENT_CODE OFF)

# Linker settings
target_link_options(kernel.bin PRIVATE
        "-T${CMAKE_CURRENT_SOURCE_DIR}/linker.ld"
        "-m64"
        "-nostdlib"
        "-no-pie"
        "-z" "max-page-size=0x1000"
)

# Create GRUB ISO using grub-mkrescue
add_custom_target(iso
        COMMAND ${CMAKE_COMMAND} -E make_directory iso/boot/grub
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:kernel.bin> iso/boot/kernel.bin
        COMMAND grub-mkrescue -o kernel.iso iso
        DEPENDS kernel.bin        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Building bootable GRUB ISO..."
)

configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/grub.cfg.in"
        "${CMAKE_CURRENT_BINARY_DIR}/iso/boot/grub/grub.cfg"
        COPYONLY
)

# Launch QEMU with the GRUB ISO
add_custom_target(run
        COMMAND qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -cdrom kernel.iso -no-reboot -M smm=off -serial stdio
        DEPENDS iso
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Booting ISO in QEMU..."
)