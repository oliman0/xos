#ifndef XOS_ACPI_H
#define XOS_ACPI_H

#include <stdint.h>

typedef struct __attribute__((packed))
{
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} acpi_header_t;

typedef struct __attribute__((packed))
{
    char     signature[8];
    uint8_t  checksum;
    char     oem_id[6];
    uint8_t  revision;
    uint32_t rsdt_address;
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t  extended_checksum;
    uint8_t  reserved[3];
} rsdp_descriptor_20_t;

typedef struct __attribute__((packed))
{
    acpi_header_t header;
    uint32_t entry[];
} acpi_rsdt_t;

typedef struct __attribute__((packed))
{
    acpi_header_t header;
    uint64_t entry[];
} acpi_xsdt_t;

typedef struct __attribute__((packed))
{
    acpi_header_t header; // Signature: "HPET"
    uint32_t event_timer_block_id;
    struct {
        uint8_t  space_id;    // 0 = System Memory (MMIO)
        uint8_t  bit_width;
        uint8_t  bit_offset;
        uint8_t  access_size;
        uint64_t address;     // HPET MMIO Base Physical Address
    } __attribute__((packed)) address;
    uint8_t  hpet_number;
    uint16_t minimum_tick;
    uint8_t  page_protection;
} acpi_hpet_table_t;

typedef struct __attribute__((packed))
{
    acpi_header_t header;
    uint32_t local_apic_address;
    uint32_t flags;
} acpi_madt_t;

typedef struct __attribute__((packed))
{
    uint8_t type;
    uint8_t length;
} acpi_madt_entry_t;

#define ACPI_MADT_TYPE_LOCAL_APIC 0
#define ACPI_MADT_TYPE_IOAPIC 1
#define ACPI_MADT_TYPE_INTERRUPT_OVERRIDE 2
#define ACPI_MADT_TYPE_NMI_SOURCE 3
#define ACPI_MADT_TYPE_LOCAL_APIC_NMI 4
#define ACPI_MADT_TYPE_LOCAL_APIC_ADDRESS_OVERRIDE 5
#define ACPI_MADT_TYPE_LOCAL_X2APIC 9

typedef struct __attribute__((packed))
{
    acpi_madt_entry_t header;
    uint8_t processor_id;
    uint8_t apic_id;
    uint32_t flags;
} acpi_madt_local_apic_t;

typedef struct __attribute__((packed))
{
    acpi_madt_entry_t header;
    uint8_t ioapic_id;
    uint8_t reserved;
    uint32_t ioapic_address;
    uint32_t global_system_interrupt_base;
} acpi_madt_ioapic_t;

typedef struct __attribute__((packed))
{
    acpi_madt_entry_t header;
    uint8_t bus;
    uint8_t irq;
    uint32_t global_system_interrupt;
    uint16_t flags;
} acpi_madt_interrupt_override_t;

void* find_acpi_table(rsdp_descriptor_20_t* rsdp, const char* signature);

#endif