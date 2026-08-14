#ifndef XOS_UEFI_LINEAR_FRAMEBUFFER_H
#define XOS_UEFI_LINEAR_FRAMEBUFFER_H

#include <stdint.h>
#include <stddef.h>

#include <kernel/boot/multiboot2.h>
#include <kernel/mem/vmm.h>
#include <kernel/lib/font.h>

void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color);

void fb_put_str(const char* str);

void fb_clear(uint32_t color);
void fb_set_back_color(uint32_t color);
void fb_set_front_color(uint32_t color);

void fb_init(multiboot_tag_framebuffer_t* framebuffer_tag);

#endif