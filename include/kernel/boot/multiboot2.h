#ifndef XOS_MULTIBOOT2_H
#define XOS_MULTIBOOT2_H

#include <stdint.h>

#define MULTIBOOT_MAGIC 0x36d76289
#define MULTIBOOT_TAG_TYPE_END 0
#define MULTIBOOT_TAG_TYPE_MMAP 6
#define MULTIBOOT_TAG_TYPE_FRAMEBUFFER 8
#define MULTIBOOT_TAG_TYPE_ACPI 15

#define MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED 0
#define MULTIBOOT_FRAMEBUFFER_TYPE_RGB     1
#define MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT     2

#define MULTIBOOT_MEMORY_AVAILABLE              1
#define MULTIBOOT_MEMORY_RESERVED               2
#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE       3
#define MULTIBOOT_MEMORY_NVS                    4
#define MULTIBOOT_MEMORY_BADRAM                 5

typedef struct
{
    uint32_t type;
    uint32_t size;
} multiboot_tag_t;

typedef struct __attribute__((packed))
{
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t zero;
} multiboot_mmap_entry_t;

typedef struct
{
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    multiboot_mmap_entry_t entries[];
} multiboot_tag_mmap_t;

typedef struct
{
    uint32_t type;
    uint32_t size;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;
    uint16_t reserved;
} multiboot_tag_framebuffer_t;

typedef struct
{
    uint32_t type;
    uint32_t size;
    uint8_t  rsdp[];
} multiboot_tag_acpi_t;

typedef struct
{
    multiboot_tag_framebuffer_t *framebuffer_tag;
    multiboot_tag_mmap_t *mmap_tag;
    multiboot_tag_acpi_t *acpi_tag;
} multiboot_tags_t;

multiboot_tags_t parse_multiboot2_tags(uint64_t multiboot2_info_addr);

#endif