#ifndef XOS_KERNEL_IO_H
#define XOS_KERNEL_IO_H

#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>

#include <kernel/drivers/uefi_linear_framebuffer.h>

void kprintf(const char *fmtstr, ...);

#endif