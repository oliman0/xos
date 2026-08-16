#include <stddef.h>
#include <kernel/drivers/linear_framebuffer.h>
#include <kernel/lib/font.h>
#include <kernel/lib/string.h>
#include <kernel/mem/pmm.h>
#include <kernel/mem/vmm.h>

typedef struct
{
    virt_addr_t vram_base;
    virt_addr_t backbuffer_base;
    virt_addr_t active_fb_base;

    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint8_t bpp;

    uint32_t cursor_x;
    uint32_t cursor_y;

    uint32_t fg_color;
    uint32_t bg_color;

    void (*put_pixel_raw)(uint32_t x, uint32_t y, uint32_t color);
    void (*clear_raw)(uint32_t color);

    uint32_t font_scale;
    uint32_t font_height;
    uint32_t font_width;
} framebuffer_state_t;

static framebuffer_state_t fb_state;

static void put_pixel_32(uint32_t x, uint32_t y, uint32_t color)
{
    uint32_t* row = (uint32_t*)(fb_state.active_fb_base + (uint64_t)y * fb_state.pitch);
    row[x] = color;
}

static void put_pixel_24(uint32_t x, uint32_t y, uint32_t color)
{
    uint8_t* pixel = (uint8_t*)(fb_state.active_fb_base + (uint64_t)y * fb_state.pitch + (uint64_t)x * 3);
    pixel[0] = color & 0xFF;         // Blue
    pixel[1] = (color >> 8) & 0xFF;  // Green
    pixel[2] = (color >> 16) & 0xFF; // Red
}

static void put_pixel_16(uint32_t x, uint32_t y, uint32_t color)
{
    uint16_t* row = (uint16_t*)(fb_state.active_fb_base + (uint64_t)y * fb_state.pitch);
    // Convert 888 RGB to 565 RGB
    uint16_t r = (color >> 19) & 0x1F;
    uint16_t g = (color >> 10) & 0x3F;
    uint16_t b = (color >> 3) & 0x1F;
    row[x] = (uint16_t)((r << 11) | (g << 5) | b);
}

static void clear_32(uint32_t color)
{
    for (uint32_t y = 0; y < fb_state.height; y++)
    {
        uint32_t* row = (uint32_t*)(fb_state.active_fb_base + (uint64_t)y * fb_state.pitch);
        for (uint32_t x = 0; x < fb_state.width; x++)
        {
            row[x] = color;
        }
    }
}

static void clear_generic(uint32_t color)
{
    for (uint32_t y = 0; y < fb_state.height; y++)
    {
        for (uint32_t x = 0; x < fb_state.width; x++)
        {
            fb_state.put_pixel_raw(x, y, color);
        }
    }
}

static void fb_scroll() {
    size_t scroll_amount = fb_state.font_height * fb_state.font_scale * fb_state.pitch;
    size_t total_size = fb_state.height * fb_state.pitch;

    // Shift everything up
    memcpy((void*)fb_state.active_fb_base, (void*)(fb_state.active_fb_base + scroll_amount), total_size - scroll_amount);

    // Clear the newly exposed last row
    for (uint64_t i = total_size - scroll_amount; i < total_size; i++)
    {
        fb_state.put_pixel_raw(i % fb_state.width, i / fb_state.pitch, fb_state.bg_color);
    }

    fb_state.cursor_y -= fb_state.font_height * fb_state.font_scale;
}

static void fb_draw_char(char c, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg)
{
    if ((unsigned char)c > 127) return;
    const uint8_t* glyph = font_8x16[(unsigned char)c];

    for (uint32_t cy = 0; cy < fb_state.font_height; cy++)
    {
        uint8_t row = glyph[cy];
        for (uint32_t cx = 0; cx < fb_state.font_width; cx++)
        {
            uint32_t color = (row & (1 << (7 - cx))) ? fg : bg;

            for (uint32_t sy = 0; sy < fb_state.font_scale; sy++)
            {
                for (uint32_t sx = 0; sx < fb_state.font_scale; sx++)
                {
                    fb_state.put_pixel_raw(x + (cx * fb_state.font_scale) + sx, y + (cy * fb_state.font_scale) + sy, color);
                }
            }
        }
    }
}

void fb_init(multiboot_tag_framebuffer_t* framebuffer_tag)
{
    fb_state.width = framebuffer_tag->framebuffer_width;
    fb_state.height = framebuffer_tag->framebuffer_height;
    fb_state.pitch = framebuffer_tag->framebuffer_pitch;
    fb_state.bpp = framebuffer_tag->framebuffer_bpp;

    size_t fb_size = fb_state.height * fb_state.pitch;
    fb_size = ALIGN_UP(fb_size, HUGE_PAGE_SIZE);

    fb_state.vram_base = PHYS_TO_VIRT(framebuffer_tag->framebuffer_addr);
    vmm_map_range(framebuffer_tag->framebuffer_addr, fb_state.vram_base, fb_size, PAGE_WRITABLE | PAGE_WC);

    fb_state.backbuffer_base = FB_BACKBUFFER_BASE;
    vmm_alloc_map_range(fb_state.backbuffer_base, fb_size, PAGE_WRITABLE);

    fb_state.active_fb_base = fb_state.backbuffer_base;

    fb_state.cursor_x = 0;
    fb_state.cursor_y = 0;
    fb_state.bg_color = 0x00000000;
    fb_state.fg_color = 0xFFFFFFFF;

    switch (fb_state.bpp)
    {
        case 32:
            fb_state.put_pixel_raw = put_pixel_32;
            fb_state.clear_raw = clear_32;
            break;
        case 24:
            fb_state.put_pixel_raw = put_pixel_24;
            fb_state.clear_raw = clear_generic;
            break;
        case 16:
            fb_state.put_pixel_raw = put_pixel_16;
            fb_state.clear_raw = clear_generic;
            break;
        default:
            fb_state.put_pixel_raw = put_pixel_32;
            fb_state.clear_raw = clear_generic;
            break;
    }

    fb_state.font_height = FONT_8x16_HEIGHT;
    fb_state.font_width = FONT_8x16_WIDTH;
    fb_state.font_scale = 1;
}

void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (x >= fb_state.width || y >= fb_state.height) return;
    fb_state.put_pixel_raw(x, y, color);
}

void fb_put_str(const char* str)
{
    for (size_t i = 0; str[i] != '\0'; i++)
    {
        if (str[i] == '\n')
        {
            fb_state.cursor_x = 0;
            fb_state.cursor_y += fb_state.font_height * fb_state.font_scale;
        }
        else
        {
            fb_draw_char(str[i], fb_state.cursor_x, fb_state.cursor_y, fb_state.fg_color, fb_state.bg_color);
            fb_state.cursor_x += fb_state.font_width * fb_state.font_scale;
            if (fb_state.cursor_x >= fb_state.width)
            {
                fb_state.cursor_x = 0;
                fb_state.cursor_y += fb_state.font_height * fb_state.font_scale;
            }
        }

        if (fb_state.cursor_y + fb_state.font_height * fb_state.font_scale >= fb_state.height)
        {
            fb_scroll();
        }
    }
}

void fb_clear(uint32_t color) {
    fb_state.bg_color = color;
    fb_state.clear_raw(color);
}


void fb_set_back_color(uint32_t color)
{
    fb_state.bg_color = color;
}

void fb_set_front_color(uint32_t color)
{
    fb_state.fg_color = color;
}

void fb_set_font_scale(uint32_t scale)
{
    fb_state.font_scale = scale;
}

void fb_swap_buffers() {
    size_t fb_size = fb_state.height * fb_state.pitch;
    memcpy((void*)fb_state.vram_base, (void*)fb_state.backbuffer_base, fb_size);
}

uint32_t fb_get_width() {
    return fb_state.width;
}

uint32_t fb_get_height() {
    return fb_state.height;
}