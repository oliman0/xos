#include <kernel/drivers/uefi_linear_framebuffer.h>

static multiboot_tag_framebuffer_t* fb_tag;

static uint64_t fb_virt_addr;

static uint32_t cursor_x = 0;
static uint32_t cursor_y = 0;

static uint32_t fg_color, bg_color;

void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color)
{
    if (x >= fb_tag->framebuffer_width || y >= fb_tag->framebuffer_height) {
        return;
    }
    if (fb_tag->framebuffer_bpp == 32)
    {
        uint32_t *fb = (uint32_t *)(fb_virt_addr + y * fb_tag->framebuffer_pitch);
        fb[x] = color;
    } else if (fb_tag->framebuffer_bpp == 24)
    {
        uint8_t *fb = (uint8_t *)(fb_virt_addr + y * fb_tag->framebuffer_pitch + x * 3);
        fb[0] = color & 0xFF;         // Blue
        fb[1] = (color >> 8) & 0xFF;  // Green
        fb[2] = (color >> 16) & 0xFF; // Red
    } else if (fb_tag->framebuffer_bpp == 16)
    {
        uint16_t *fb = (uint16_t *)(fb_virt_addr + y * fb_tag->framebuffer_pitch);
        // Convert 888 RGB to 565 RGB
        uint16_t r = (color >> 19) & 0x1F;
        uint16_t g = (color >> 10) & 0x3F;
        uint16_t b = (color >> 3) & 0x1F;
        fb[x] = (r << 11) | (g << 5) | b;
    }
}

static void fb_draw_char(char c, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg)
{
    if ((unsigned char)c > 127) return;
    const uint8_t* glyph = font_8x16[(unsigned char)c];

    for (int cy = 0; cy < 16; cy++)
    {
        uint8_t row = glyph[cy];
        for (int cx = 0; cx < 8; cx++)
        {
            if (row & (1 << (7 - cx)))
            {
                fb_put_pixel(x + cx, y + cy, fg);
            } else
            {
                fb_put_pixel(x + cx, y + cy, bg);
            }
        }
    }
}

void fb_put_str(const char* str)
{
    for (size_t i = 0; str[i] != '\0'; i++)
    {
        if (str[i] == '\n')
        {
            cursor_x = 0;
            cursor_y += 16;
        } else
        {
            fb_draw_char(str[i], cursor_x, cursor_y, fg_color, bg_color);
            cursor_x += 8;
            if (cursor_x >= fb_tag->framebuffer_width)
            {
                cursor_x = 0;
                cursor_y += 16;
            }
        }
    }
}

void fb_clear(uint32_t color)
{
    bg_color = color;

    if (fb_tag->framebuffer_bpp == 32)
    {
        for (uint32_t y = 0; y < fb_tag->framebuffer_height; y++)
        {
            uint32_t *fb = (uint32_t *)(fb_virt_addr + y * fb_tag->framebuffer_pitch);
            for (uint32_t x = 0; x < fb_tag->framebuffer_width; x++)
            {
                fb[x] = color;
            }
        }
    } else {
        // Fallback for 16/24 bpp
        for (uint32_t y = 0; y < fb_tag->framebuffer_height; y++)
        {
            for (uint32_t x = 0; x < fb_tag->framebuffer_width; x++)
            {
                fb_put_pixel(x, y, bg_color);
            }
        }
    }
}

void fb_set_back_color(uint32_t color)
{
    bg_color = color;
}

void fb_set_front_color(uint32_t color)
{
    fg_color = color;
}

void fb_init(multiboot_tag_framebuffer_t* framebuffer_tag) {
    fb_tag = framebuffer_tag;

    fb_virt_addr = PHYS_TO_VIRT(fb_tag->framebuffer_addr);

    uint64_t fb_size = (uint64_t)fb_tag->framebuffer_height * fb_tag->framebuffer_pitch;
    // Round up to a full 2MiB page so the last scanlines are always mapped
    fb_size = (fb_size + HUGE_PAGE_SIZE - 1) & ~((uint64_t)HUGE_PAGE_SIZE - 1);

    vmm_map_range(fb_tag->framebuffer_addr, fb_virt_addr, fb_size, PAGE_WRITABLE | PAGE_CACHE_DISABLE);

    bg_color = 0x00000000;
    fg_color = 0xffffffff;
}