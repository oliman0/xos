#include <stddef.h>
#include <kernel/boot/multiboot2.h>
#include <kernel/mem/ptr.h>

multiboot_info_table_t parse_multiboot2_tags(virt_addr_t multiboot2_info_addr)
{
    multiboot_info_table_t info_table = {0};

    // Ensure pointer is non-null and 8-byte aligned
    if (!multiboot2_info_addr || (multiboot2_info_addr & 7) != 0) {
        return info_table;
    }

    // Multiboot2 header: First 4 bytes = total_size, next 4 bytes = reserved
    size_t total_size = *(uint32_t *)multiboot2_info_addr;
    uint8_t *end_addr = (uint8_t *)multiboot2_info_addr + total_size;

    // Tags start 8 bytes after the base address
    multiboot_tag_t *tag = (multiboot_tag_t *)(multiboot2_info_addr + 8);

    // Safely loop while inside bounds and before MULTIBOOT_TAG_TYPE_END
    while ((uint8_t *)tag < end_addr && tag->type != MULTIBOOT_TAG_TYPE_END)
    {
        // Guard against corrupt tags that would cause an infinite loop
        if (tag->size < 8) {
            break;
        }

        switch (tag->type)
        {
            case MULTIBOOT_TAG_TYPE_FRAMEBUFFER:
                info_table.framebuffer_tag = (multiboot_tag_framebuffer_t *)tag;
                break;

            case MULTIBOOT_TAG_TYPE_MMAP:
                info_table.mmap_tag = (multiboot_tag_mmap_t *)tag;
                break;

            case MULTIBOOT_TAG_TYPE_ACPI:
                info_table.acpi_tag = (multiboot_tag_acpi_t *)tag;
                break;
        }

        // Advance to the next tag (8-byte aligned)
        tag = (multiboot_tag_t *)((uint8_t *)tag + ((tag->size + 7) & ~7U));
    }

    return info_table;
}
