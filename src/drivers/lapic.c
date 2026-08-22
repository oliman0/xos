#include <kernel/drivers/lapic.h>
#include <kernel/arch/io.h>
#include <kernel/mem/vmm.h>
#include <kernel/drivers/acpi.h>

static uint64_t lapic_base = 0;
static uint64_t hpet_base = 0;

static uint32_t get_lapic_ticks_per_ms_cpuid()
{
    uint32_t eax, ebx, ecx, edx;

    // Check maximum supported CPUID leaf
    __asm__ __volatile__("cpuid" : "=a"(eax) : "a"(0));
    if (eax < CPUID_LEAF_TSC) return 0;

    __asm__ __volatile__(
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(CPUID_LEAF_TSC)
    );

    if (ecx != 0) {
        return (ecx / 1000) / 16;
    }

    // Fallback: CPU does not report crystal clock frequency directly
    return 0;
}

static uint32_t get_lapic_ticks_per_ms_hpet()
{
    if (hpet_base == 0) return 0;

    // Period is in bits 32-63, in femtoseconds (10^-15)
    uint32_t period = mmio_read(hpet_base, ACPI_HPET_REG_CAPABILITIES) >> 32;

    // We want to wait 10ms = 10^13 femtoseconds
    uint64_t ticks_to_wait = 10000000000000ULL / period;

    // Enable HPET counter. Bit 0 is 'overall enable'.
    uint64_t hpet_config = mmio_read(hpet_base, ACPI_HPET_REG_CONFIGURATION);
    mmio_write(hpet_base, ACPI_HPET_REG_CONFIGURATION, hpet_config | 1);

    // Set LAPIC divisor to 16
    mmio_write(lapic_base, LAPIC_TDCR_REG, LAPIC_TIMER_DIV_16);

    // Initial count
    uint32_t start_lapic = 0xFFFFFFFF;
    mmio_write(lapic_base, LAPIC_TICR_REG, start_lapic);

    uint64_t hpet_start = mmio_read(hpet_base, ACPI_HPET_REG_MAIN_COUNTER);
    while (mmio_read(hpet_base, ACPI_HPET_REG_MAIN_COUNTER) - hpet_start < ticks_to_wait) {
        __asm__ volatile ("pause");
    }

    uint32_t end_lapic = mmio_read(lapic_base, LAPIC_TCCR_REG);
    uint32_t ticks_passed = start_lapic - end_lapic;

    return ticks_passed / 10;
}

uint32_t get_lapic_ticks_per_ms()
{
    uint32_t freq = get_lapic_ticks_per_ms_cpuid();

    if (freq == 0)
    {
        freq = get_lapic_ticks_per_ms_hpet();
    }

    return freq;
}

void lapic_timer_start_periodic(uint32_t frequency_hz, uint32_t ticks_per_ms, uint8_t vector)
{
    mmio_write(lapic_base, LAPIC_TDCR_REG, LAPIC_TIMER_DIV_16);

    mmio_write(lapic_base, LAPIC_LVT_TIMER_REG, LAPIC_TIMER_MODE_PERIODIC | vector);

    uint32_t init_count = (ticks_per_ms * 1000) / frequency_hz;

    // start countdown
    mmio_write(lapic_base, LAPIC_TICR_REG, init_count);
}

void lapic_init()
{
    uint64_t apic_msr = rdmsr(IA32_APIC_BASE_MSR);

    // If x2APIC is enabled, we must disable it to use the MMIO interface (xAPIC).
    // The transition from x2APIC to xAPIC requires going through the disabled state.
    if (apic_msr & IA32_APIC_BASE_MSR_X2APIC) {
        apic_msr &= ~(IA32_APIC_BASE_MSR_ENABLE | IA32_APIC_BASE_MSR_X2APIC);
        wrmsr(IA32_APIC_BASE_MSR, apic_msr);

        apic_msr |= IA32_APIC_BASE_MSR_ENABLE;
        wrmsr(IA32_APIC_BASE_MSR, apic_msr);
    }

    // Enable Local APIC globally via MSR (if not already enabled)
    if (!(apic_msr & IA32_APIC_BASE_MSR_ENABLE)) {
        apic_msr |= IA32_APIC_BASE_MSR_ENABLE;
        wrmsr(IA32_APIC_BASE_MSR, apic_msr);
    }

    // Read the physical base of the Local APIC MMIO
    uint64_t apic_phys_base = apic_msr & IA32_APIC_BASE_MSR_MASK;

    lapic_base = PHYS_TO_VIRT(apic_phys_base);

    // Map the LAPIC MMIO as uncacheable. vmm_map_page_4kb now handles huge page splitting.
    vmm_map_page_4kb(apic_phys_base, lapic_base, PAGE_WRITABLE | PAGE_UC);

    acpi_hpet_table_t* hpet_table = (acpi_hpet_table_t*)acpi_find_table(ACPI_SIGNATURE_HPET);
    if (hpet_table) {
        uint64_t hpet_phys = hpet_table->address.address;
        hpet_base = PHYS_TO_VIRT(hpet_phys);
        vmm_map_page_4kb(hpet_phys, hpet_base, PAGE_WRITABLE | PAGE_UC);
    }

    // Set Task Priority Register to 0 (accept all interrupts)
    mmio_write(lapic_base, LAPIC_TPR_REG, 0);

    // Enable Local APIC in Software & assign Spurious Vector (0xFF / 255)
    // Bit 8 = Software Enable Bit
    mmio_write(lapic_base, LAPIC_SVR_REG, 0x100 | 0xFF);

    __asm__ __volatile("sti");
}

void lapic_eoi()
{
    mmio_write(lapic_base, LAPIC_EOI_REG, 0);
}