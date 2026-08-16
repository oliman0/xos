#include <kernel/drivers/acpi.h>
#include <kernel/lib/string.h>
#include <kernel/mem/vmm.h>
#include <stdbool.h>

static bool acpi_checksum(acpi_header_t* table, uint32_t length)
{
    uint8_t sum = 0;
    uint8_t* ptr = (uint8_t*)table;
    for (uint32_t i = 0; i < length; i++)
    {
        sum += ptr[i];
    }
    return sum == 0;
}

acpi_header_t* find_acpi_table(rsdp_descriptor_20_t* rsdp, const char* signature)

{
    if (rsdp->revision >= 2 && rsdp->xsdt_address)
    {
        acpi_xsdt_t* xsdt = (acpi_xsdt_t*)PHYS_TO_VIRT(rsdp->xsdt_address);
        int entries = (xsdt->header.length - sizeof(acpi_header_t)) / 8;
        for (int i = 0; i < entries; i++)
        {
            acpi_header_t* header = (acpi_header_t*)PHYS_TO_VIRT(xsdt->entry[i]);
            if (memcmp(header->signature, signature, 4) == 0)
            {
                if (acpi_checksum(header, header->length)) return header;
            }
        }
    } else
    {
        acpi_rsdt_t* rsdt = (acpi_rsdt_t*)PHYS_TO_VIRT(rsdp->rsdt_address);
        int entries = (rsdt->header.length - sizeof(acpi_header_t)) / 4;
        for (int i = 0; i < entries; i++)
        {
            acpi_header_t* header = (acpi_header_t*)PHYS_TO_VIRT(rsdt->entry[i]);
            if (memcmp(header->signature, signature, 4) == 0)
            {
                if (acpi_checksum(header, header->length)) return header;
            }
        }
    }
    return NULL;
}