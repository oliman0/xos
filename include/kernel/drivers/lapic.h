#ifndef XOS_LAPIC_H
#define XOS_LAPIC_H

#include <stdint.h>
#include <kernel/boot/multiboot2.h>

// LAPIC Register MMIO Offsets
#define LAPIC_ID_REG      0x020
#define LAPIC_VER_REG     0x030
#define LAPIC_TPR_REG     0x080
#define LAPIC_EOI_REG     0x0B0
#define LAPIC_SVR_REG     0x0F0
#define LAPIC_ESR_REG     0x280
#define LAPIC_LVT_TIMER_REG   0x320
#define LAPIC_TICR_REG    0x380
#define LAPIC_TCCR_REG    0x390
#define LAPIC_TDCR_REG    0x3E0

#define IA32_APIC_BASE_MSR 0x1B
#define IA32_APIC_BASE_MSR_MASK 0x000FFFFFFFFFF000ULL
#define IA32_APIC_BASE_MSR_ENABLE (1 << 11)
#define IA32_APIC_BASE_MSR_X2APIC (1 << 10)

// Timer Modes & Flags (LVT Timer Register)
#define LAPIC_TIMER_MODE_PERIODIC (1 << 17)
#define LAPIC_TIMER_MODE_ONESHOT  (0 << 17)
#define LAPIC_TIMER_MASKED        (1 << 16)

// Divisor Values (TDCR Register)
#define LAPIC_TIMER_DIV_1   0x0B
#define LAPIC_TIMER_DIV_16  0x03
#define LAPIC_TIMER_DIV_128 0x0A

#define LAPIC_TIMER_VECTOR 32

#define PIT_BASE_FREQUENCY   1193182

void lapic_init(multiboot_tag_acpi_t* acpi_tag);
void lapic_eoi();

uint32_t get_lapic_ticks_per_ms();
void lapic_timer_start_periodic(uint32_t frequency_hz, uint32_t ticks_per_ms, uint8_t vector);

#endif