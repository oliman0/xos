#ifndef XOS_linear_framebuffer_H
#define XOS_linear_framebuffer_H

#include <stdint.h>

#include <kernel/boot/multiboot2.h>

#define FB_BACKBUFFER_BASE 0xFFFF900000000000ULL

void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color);

void fb_put_str(const char* str);

void fb_clear(uint32_t color);

void fb_set_back_color(uint32_t color);
void fb_set_front_color(uint32_t color);

void fb_set_font_scale(uint32_t scale);

void fb_swap_buffers();

uint32_t fb_get_width();
uint32_t fb_get_height();

void fb_init(multiboot_tag_framebuffer_t* framebuffer_tag);

#endif