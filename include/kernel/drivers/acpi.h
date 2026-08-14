#ifndef XOS_ACPI_H
#define XOS_ACPI_H

#include <stdint.h>

typedef struct __attribute__((packed)) {
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

typedef struct __attribute__((packed)) {
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

typedef struct __attribute__((packed)) {
    acpi_header_t header;
    uint32_t entry[];
} acpi_rsdt_t;

typedef struct __attribute__((packed)) {
    acpi_header_t header;
    uint64_t entry[];
} acpi_xsdt_t;

typedef struct __attribute__((packed)) {
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

void* find_acpi_table(rsdp_descriptor_20_t* rsdp, const char* signature);

#endif