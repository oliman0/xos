#ifndef XOS_IOAPIC_H
#define XOS_IOAPIC_H

#include <stdint.h>
#include <kernel/boot/multiboot2.h>

#define IOAPIC_REG_SEL                0x00
#define IOAPIC_REG_WIN                0x10

#define IOAPIC_REG_ID                     0x00
#define IOAPIC_REG_VER                    0x01
#define IOAPIC_REG_ARB                    0x02
#define IOAPIC_REG_REDTBL                 0x10

#define IOAPIC_REDTBL_DELMOD_FIXED    (0x000 << 8)
#define IOAPIC_REDTBL_DESTMOD_PHYS    (0 << 11)
#define IOAPIC_REDTBL_INTPOL          (1 << 13)
#define IOAPIC_REDTBL_REMOTE_IRR      (1 << 14)
#define IOAPIC_REDTBL_TRIGGER         (1 << 15)
#define IOAPIC_REDTBL_MASKED          (1 << 16)

#define IOAPIC_REDTBL_DEST_SHIFT      24

#define IOAPIC_INTERRUPT_OVERRIDE_LIMIT 16

typedef struct {
    uint8_t irq;
    uint32_t gsi;
    uint16_t flags;
} interrupt_override_t;

void ioapic_init(multiboot_tag_acpi_t* acpi_tag);
void ioapic_set_irq(uint8_t irq, uint64_t apic_id, uint8_t vector);

#endif
