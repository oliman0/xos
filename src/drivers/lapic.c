#include <kernel/drivers/lapic.h>
#include <kernel/arch/io.h>
#include <kernel/mem/vmm.h>
#include <kernel/drivers/acpi.h>

static uintptr_t lapic_base = 0;
static uintptr_t hpet_base = 0;

static inline uint32_t lapic_read(uint32_t reg)
{
    return *(volatile uint32_t*)(lapic_base + reg);
}

static inline void lapic_write(uint32_t reg, uint32_t val)
{
    *(volatile uint32_t*)(lapic_base + reg) = val;
}

static uint32_t get_lapic_ticks_per_ms_cpuid(void)
{
    uint32_t eax, ebx, ecx, edx;

    // Check maximum supported CPUID leaf
    __asm__ __volatile__("cpuid" : "=a"(eax) : "a"(0));
    if (eax < 0x15) return 0;

    // CPUID Leaf 0x15: Time Stamp Counter and Nominal Core Crystal Clock Information
    __asm__ __volatile__(
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(0x15)
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

    volatile uint64_t* hpet = (volatile uint64_t*)hpet_base;

    // HPET capabilities register at 0x00
    // Period is in bits 32-63, in femtoseconds (10^-15)
    uint32_t period = hpet[0] >> 32;

    // We want to wait 10ms = 10^13 femtoseconds
    uint64_t ticks_to_wait = 10000000000000ULL / period;

    // Enable HPET counter: Configuration register at 0x10. Bit 0 is 'overall enable'.
    hpet[2] |= 1;

    // Set LAPIC divisor to 16
    lapic_write(LAPIC_TDCR_REG, LAPIC_TIMER_DIV_16);

    // Initial count
    uint32_t start_lapic = 0xFFFFFFFF;
    lapic_write(LAPIC_TICR_REG, start_lapic);

    uint64_t start_hpet = hpet[30]; // Main counter at 0xF0
    while (hpet[30] - start_hpet < ticks_to_wait) {
        __asm__ volatile ("pause");
    }

    uint32_t end_lapic = lapic_read(LAPIC_TCCR_REG);
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
    lapic_write(LAPIC_TDCR_REG, LAPIC_TIMER_DIV_16);

    lapic_write(LAPIC_LVT_TIMER_REG, LAPIC_TIMER_MODE_PERIODIC | vector);

    uint32_t period_ms = 1000 / frequency_hz;
    uint32_t init_count = period_ms * ticks_per_ms;

    // start countdown
    lapic_write(LAPIC_TICR_REG, init_count);
}

void lapic_init(multiboot_tag_acpi_t* acpi_tag)
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
    vmm_map_page_4kb(apic_phys_base, lapic_base, PAGE_WRITABLE | PAGE_CACHE_DISABLE);

    if (acpi_tag) {
        rsdp_descriptor_20_t* rsdp = (rsdp_descriptor_20_t*)acpi_tag->rsdp;
        acpi_hpet_table_t* hpet_table = (acpi_hpet_table_t*)find_acpi_table(rsdp, "HPET");
        if (hpet_table) {
            uint64_t hpet_phys = hpet_table->address.address;
            hpet_base = PHYS_TO_VIRT(hpet_phys);
            vmm_map_page_4kb(hpet_phys, hpet_base, PAGE_WRITABLE | PAGE_CACHE_DISABLE);
        }
    }

    // Set Task Priority Register to 0 (accept all interrupts)
    lapic_write(LAPIC_TPR_REG, 0);

    // Enable Local APIC in Software & assign Spurious Vector (0xFF / 255)
    // Bit 8 = Software Enable Bit
    lapic_write(LAPIC_SVR_REG, 0x100 | 0xFF);

    __asm__ __volatile("sti");
}

void lapic_eoi()
{
    lapic_write(LAPIC_EOI_REG, 0);
}