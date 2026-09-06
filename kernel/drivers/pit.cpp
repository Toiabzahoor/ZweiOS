#include "drivers/pit.hpp"
#include "arch/x86_64/io.hpp"
#include "arch/x86_64/pic.hpp"
#include "arch/x86_64/isr.hpp"
#include "drivers/serial.hpp"
#include "proc/sched.hpp"

namespace drivers {

static constexpr uint16_t PIT_CHANNEL0_DATA = 0x40;
static constexpr uint16_t PIT_COMMAND_PORT  = 0x43;

static volatile uint64_t g_pit_ticks = 0;
static uint32_t g_pit_frequency = PIT_TARGET_FREQUENCY;

static inline bool interrupts_enabled() {
    uint64_t rflags;
    asm volatile("pushfq; popq %0" : "=r"(rflags));
    return (rflags & (1 << 9)) != 0;
}

void pit_irq_handler(arch::cpu_registers_t* regs) {
    g_pit_ticks++;
    arch::pic_send_eoi(0);
    proc::sched_tick(regs);
}

void pit_init(uint32_t frequency) {
    if (frequency == 0) frequency = PIT_TARGET_FREQUENCY;
    g_pit_frequency = frequency;
    g_pit_ticks = 0;

    uint32_t divisor = PIT_BASE_FREQUENCY / frequency;
    if (divisor > 65535) divisor = 65535;
    if (divisor == 0) divisor = 1;

    arch::outb(PIT_COMMAND_PORT, 0x36);
    arch::io_wait();
    arch::outb(PIT_CHANNEL0_DATA, static_cast<uint8_t>(divisor & 0xFF));
    arch::io_wait();
    arch::outb(PIT_CHANNEL0_DATA, static_cast<uint8_t>((divisor >> 8) & 0xFF));
    arch::io_wait();

    arch::isr_register_handler(32, pit_irq_handler);
    arch::pic_clear_mask(0);

    drivers::serial_puts("[PIT] 8254 Timer initialized at 100 Hz (IRQ0 mapped to vector 32)\r\n");
}

uint64_t timer_get_ticks() {
    return g_pit_ticks;
}

uint64_t timer_get_uptime_seconds() {
    if (g_pit_frequency == 0) return 0;
    return g_pit_ticks / g_pit_frequency;
}

void timer_get_uptime(uint32_t* hours, uint32_t* minutes, uint32_t* seconds) {
    uint64_t total_sec = timer_get_uptime_seconds();
    if (hours)   *hours   = static_cast<uint32_t>(total_sec / 3600);
    if (minutes) *minutes = static_cast<uint32_t>((total_sec % 3600) / 60);
    if (seconds) *seconds = static_cast<uint32_t>(total_sec % 60);
}

void timer_sleep_ticks(uint64_t ticks) {
    uint64_t start = g_pit_ticks;
    uint64_t target = start + ticks;

    while (g_pit_ticks < target) {
        if (interrupts_enabled()) {
            asm volatile("hlt");
        } else {
            asm volatile("pause");
        }
    }
}

void timer_sleep_seconds(uint32_t seconds) {
    timer_sleep_ticks(static_cast<uint64_t>(seconds) * g_pit_frequency);
}

}
