#ifndef XOS_ACPI_H
#define XOS_ACPI_H

#include <stdint.h>
#include <kernel/boot/multiboot2.h>

typedef enum
{
    ACPI_SIGNATURE_MADT = 0,
    ACPI_SIGNATURE_HPET = 1,
    ACPI_SIGNATURE_FADT = 2,

} acpi_table_signature_t;

#define ACPI_MADT_ISO_POLARITY_MASK         0x3
#define ACPI_MADT_ISO_POLARITY_BUS_DEFAULT  0x0
#define ACPI_MADT_ISO_POLARITY_ACTIVE_HIGH  0x1
#define ACPI_MADT_ISO_POLARITY_ACTIVE_LOW   0x3

#define ACPI_MADT_ISO_TRIGGER_MASK          0x3
#define ACPI_MADT_ISO_TRIGGER_SHIFT         2
#define ACPI_MADT_ISO_TRIGGER_BUS_DEFAULT   0x0
#define ACPI_MADT_ISO_TRIGGER_EDGE          0x1
#define ACPI_MADT_ISO_TRIGGER_LEVEL         0x3

#define ACPI_HPET_REG_CAPABILITIES 0x00
#define ACPI_HPET_REG_CONFIGURATION 0x10
#define ACPI_HPET_REG_MAIN_COUNTER 0xF0

#define ACPI_MADT_TYPE_LOCAL_APIC 0
#define ACPI_MADT_TYPE_IOAPIC 1
#define ACPI_MADT_TYPE_INTERRUPT_OVERRIDE 2
#define ACPI_MADT_TYPE_NMI_SOURCE 3
#define ACPI_MADT_TYPE_LOCAL_APIC_NMI 4
#define ACPI_MADT_TYPE_LOCAL_APIC_ADDRESS_OVERRIDE 5
#define ACPI_MADT_TYPE_LOCAL_X2APIC 9

#define ACPI_FADT_PM1_SLP_TYP_SHIFT  10
#define ACPI_FADT_PM1_SLP_EN         (1 << 13)
#define ACPI_FADT_S5_SLP_TYP_DEFAULT 5
#define ACPI_FADT_IAPC_8042_FLAG (1 << 1)

#define AML_OP_PACKAGE          0x12
#define AML_OP_BYTE_PREFIX      0x0A

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

typedef struct __attribute__((packed)) {
    uint8_t  space_id;       // 0 = System Memory (MMIO), 1 = System I/O
    uint8_t  bit_width;
    uint8_t  bit_offset;
    uint8_t  access_size;
    uint64_t address;
} acpi_gas_t;

typedef struct __attribute__((packed))
{
    acpi_header_t header; // Signature: "HPET"
    uint32_t event_timer_block_id;
    acpi_gas_t address;
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

typedef struct __attribute__((packed)) {
    acpi_madt_entry_t header;
    uint8_t  processor_id; // 0xFF = All Processors
    uint16_t flags;
    uint8_t  lint;         // LINT0 or LINT1
} acpi_madt_lapic_nmi_t;

// Type 5: 64-bit Local APIC Address Override
typedef struct __attribute__((packed)) {
    acpi_madt_entry_t header;
    uint16_t reserved;
    uint64_t local_apic_address;
} acpi_madt_lapic_override_t;

typedef struct __attribute__((packed)) {
    acpi_header_t header;
    uint32_t firmware_ctrl;
    uint32_t dsdt;
    uint8_t  reserved;
    uint8_t  preferred_pm_profile;
    uint16_t sci_interrupt;
    uint32_t smi_command_port;
    uint8_t  acpi_enable;
    uint8_t  acpi_disable;
    uint8_t  s4bios_req;
    uint8_t  pstate_control;
    uint32_t pm1a_event_block;
    uint32_t pm1b_event_block;
    uint32_t pm1a_control_block;
    uint32_t pm1b_control_block;
    uint32_t pm2_control_block;
    uint32_t pm_timer_block;
    uint32_t gpe0_block;
    uint32_t gpe1_block;
    uint8_t  pm1_event_length;
    uint8_t  pm1_control_length;
    uint8_t  pm2_control_length;
    uint8_t  pm_timer_length;
    uint8_t  gpe0_block_length;
    uint8_t  gpe1_block_length;
    uint8_t  gpe1_base;
    uint8_t  cstate_control;
    uint16_t worst_c2_latency;
    uint16_t worst_c3_latency;
    uint16_t flush_size;
    uint16_t flush_stride;
    uint8_t  duty_offset;
    uint8_t  duty_width;
    uint8_t  day_alarm;
    uint8_t  month_alarm;
    uint8_t  century;
    uint16_t boot_architecture_flags;
    uint8_t  reserved2;
    uint32_t flags;
    acpi_gas_t reset_reg;
    uint8_t  reset_value;
    uint8_t  reserved3[3];
    uint64_t x_firmware_control;
    uint64_t x_dsdt;
    acpi_gas_t x_pm1a_event_block;
    acpi_gas_t x_pm1b_event_block;
    acpi_gas_t x_pm1a_control_block;
    acpi_gas_t x_pm1b_control_block;
} acpi_fadt_t;

void acpi_init(multiboot_tag_acpi_t* acpi_tag);

acpi_header_t* acpi_find_table(acpi_table_signature_t signature);

void acpi_shutdown();

#endif