#include <kernel/drivers/ioapic.h>
#include <kernel/drivers/acpi.h>
#include <kernel/mem/vmm.h>
#include <kernel/kernel_io.h>
#include <stddef.h>
#include <kernel/mem/ptr.h>

static virt_addr_t ioapic_base = 0;

static interrupt_override_t overrides[IOAPIC_INTERRUPT_OVERRIDE_LIMIT];
static int override_count = 0;

static void ioapic_write(uint32_t reg, uint32_t value)
{
    *(volatile uint32_t*)(ioapic_base) = reg;
    *(volatile uint32_t*)(ioapic_base + IOAPIC_REG_WIN) = value;
}

static uint32_t ioapic_read(uint32_t reg)
{
    *(volatile uint32_t*)(ioapic_base) = reg;
    return *(volatile uint32_t*)(ioapic_base + IOAPIC_REG_WIN);
}

static void ioapic_parse_madt(acpi_madt_t* madt)
{
    uint8_t* ptr = (uint8_t*)(madt + 1);
    uint8_t* end = (uint8_t*)madt + madt->header.length;

    while (ptr < end) 
    {
        acpi_madt_entry_t* entry = (acpi_madt_entry_t*)ptr;
        if (entry->type == ACPI_MADT_TYPE_IOAPIC) 
        {
            acpi_madt_ioapic_t* ioapic = (acpi_madt_ioapic_t*)entry;

            ioapic_base = PHYS_TO_VIRT(ioapic->ioapic_address);
            vmm_map_page_4kb(ioapic->ioapic_address, ioapic_base, PAGE_WRITABLE | PAGE_UC);
        }
        else if (entry->type == ACPI_MADT_TYPE_INTERRUPT_OVERRIDE)
        {
            acpi_madt_interrupt_override_t* iso = (acpi_madt_interrupt_override_t*)entry;
            if (override_count < IOAPIC_INTERRUPT_OVERRIDE_LIMIT)
            {
                overrides[override_count].irq = iso->irq;
                overrides[override_count].gsi = iso->global_system_interrupt;
                overrides[override_count].flags = iso->flags;
                override_count++;
            }
        }
        ptr += entry->length;
    }
}

void ioapic_init(multiboot_tag_acpi_t* acpi_tag)
{
    rsdp_descriptor_20_t* rsdp = (rsdp_descriptor_20_t*)acpi_tag->rsdp;
    acpi_madt_t* madt = (acpi_madt_t*)find_acpi_table(rsdp, "APIC");

    if (madt) ioapic_parse_madt(madt);
}

void ioapic_set_irq(uint8_t irq, uint64_t apic_id, uint8_t vector)
{
    if (ioapic_base == 0) return;

    uint32_t gsi = irq;
    uint16_t flags = 0;
    
    for (int i = 0; i < override_count; i++)
    {
        if (overrides[i].irq == irq)
        {
            gsi = overrides[i].gsi;
            flags = overrides[i].flags;
            break;
        }
    }

    uint32_t low = vector;
    
    // Polarity: bit 1 of flags. 0 = same as bus, 1 = active high, 3 = active low.
    // Trigger Mode: bit 3 of flags. 0 = same as bus, 1 = edge, 3 = level.
    // For ISA (where PS/2 is), default is Active High, Edge Triggered.
    
    uint8_t polarity = flags & ACPI_MADT_ISO_POLARITY_MASK;
    uint8_t trigger = (flags >> ACPI_MADT_ISO_TRIGGER_SHIFT) & ACPI_MADT_ISO_TRIGGER_MASK;

    if (polarity == ACPI_MADT_ISO_POLARITY_ACTIVE_LOW)
        low |= IOAPIC_REDTBL_INTPOL;

    if (trigger == ACPI_MADT_ISO_TRIGGER_LEVEL)
        low |= IOAPIC_REDTBL_TRIGGER;
    else if (trigger == ACPI_MADT_ISO_TRIGGER_EDGE)
        low &= ~IOAPIC_REDTBL_TRIGGER;
    
    uint32_t high = (uint32_t)(apic_id << IOAPIC_REDTBL_DEST_SHIFT);

    ioapic_write(IOAPIC_REG_REDTBL + gsi * 2, low);
    ioapic_write(IOAPIC_REG_REDTBL + gsi * 2 + 1, high);
}
