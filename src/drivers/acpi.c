#include <kernel/drivers/acpi.h>
#include <kernel/lib/string.h>
#include <kernel/mem/vmm.h>
#include <stdbool.h>

#include "kernel/arch/io.h"

static rsdp_descriptor_20_t* rsdp;

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

void acpi_init(multiboot_tag_acpi_t* acpi_tag)
{
    if (!acpi_tag) return;

    rsdp = (rsdp_descriptor_20_t*)acpi_tag->rsdp;
}

static const char* acpi_enum_to_signature[3] = { "ACPI", "HPET", "FACP" };

acpi_header_t* acpi_find_table(acpi_table_signature_t signature)
{
    if (rsdp == NULL) return NULL;

    const char* acpi_signature = acpi_enum_to_signature[signature];

    if (rsdp->revision >= 2 && rsdp->xsdt_address)
    {
        acpi_xsdt_t* xsdt = (acpi_xsdt_t*)PHYS_TO_VIRT(rsdp->xsdt_address);
        uint64_t entries = (xsdt->header.length - sizeof(acpi_header_t)) / sizeof(uint64_t);
        for (uint64_t i = 0; i < entries; i++)
        {
            acpi_header_t* header = (acpi_header_t*)PHYS_TO_VIRT(xsdt->entry[i]);
            if (memcmp(header->signature, acpi_signature, 4) == 0)
            {
                if (acpi_checksum(header, header->length)) return header;
            }
        }
    } else
    {
        acpi_rsdt_t* rsdt = (acpi_rsdt_t*)PHYS_TO_VIRT(rsdp->rsdt_address);
        uint64_t entries = (rsdt->header.length - sizeof(acpi_header_t)) / sizeof(uint32_t);
        for (uint64_t i = 0; i < entries; i++)
        {
            acpi_header_t* header = (acpi_header_t*)PHYS_TO_VIRT(rsdt->entry[i]);
            if (memcmp(header->signature, acpi_signature, 4) == 0)
            {
                if (acpi_checksum(header, header->length)) return header;
            }
        }
    }
    return NULL;
}

void acpi_shutdown()
{
    acpi_fadt_t* fadt = (acpi_fadt_t*)acpi_find_table(ACPI_SIGNATURE_FADT);
    if (fadt == NULL) return;

    uint64_t dsdt_addr = (rsdp->revision >= 2 && fadt->x_dsdt) ? fadt->x_dsdt : fadt->dsdt;
    acpi_header_t* dsdt = (acpi_header_t*)PHYS_TO_VIRT(dsdt_addr);
    if (!dsdt) return;

    char* aml = (char*)dsdt + sizeof(acpi_header_t);
    uint32_t aml_len = dsdt->length - sizeof(acpi_header_t);

    uint16_t slp_typa = ACPI_FADT_S5_SLP_TYP_DEFAULT << ACPI_FADT_PM1_SLP_TYP_SHIFT;
    uint16_t slp_typb = ACPI_FADT_S5_SLP_TYP_DEFAULT << ACPI_FADT_PM1_SLP_TYP_SHIFT;

    const char* s5_name = "_S5_";
    size_t s5_name_len = 4;

    for (uint32_t i = 0; i < aml_len - s5_name_len; i++)
    {
        if (memcmp(&aml[i], s5_name, s5_name_len) == 0)
        {
            char* ptr = &aml[i + s5_name_len];

            if (*ptr == AML_OP_PACKAGE) ptr += 2; // Skip PackageOp and pkglength
            else ptr++;

            ptr++; // Skip NumElements

            if (*ptr == AML_OP_BYTE_PREFIX) ptr++;
            slp_typa = (*ptr++) << ACPI_FADT_PM1_SLP_TYP_SHIFT;

            if (*ptr == AML_OP_BYTE_PREFIX) ptr++;
            slp_typb = (*ptr++) << ACPI_FADT_PM1_SLP_TYP_SHIFT;
            break;
        }
    }

    // Write SLP_TYPx | SLP_EN directly to PM1 control registers
    outw((uint16_t)fadt->pm1a_control_block, slp_typa | ACPI_FADT_PM1_SLP_EN);
    if (fadt->pm1b_control_block)
    {
        outw((uint16_t)fadt->pm1b_control_block, slp_typb | ACPI_FADT_PM1_SLP_EN);
    }
}