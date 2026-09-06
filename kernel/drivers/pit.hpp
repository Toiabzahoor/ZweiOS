#pragma once

#include <stdint.h>
#include <stddef.h>
#include "arch/x86_64/isr.hpp"

namespace drivers {

inline constexpr uint32_t PIT_BASE_FREQUENCY   = 1193182;
inline constexpr uint32_t PIT_TARGET_FREQUENCY = 100;

void pit_init(uint32_t frequency = PIT_TARGET_FREQUENCY);
void pit_irq_handler(arch::cpu_registers_t* regs);

uint64_t timer_get_ticks();
inline uint64_t pit_get_ticks() { return timer_get_ticks(); }
uint64_t timer_get_uptime_seconds();
void timer_get_uptime(uint32_t* hours, uint32_t* minutes, uint32_t* seconds);
void timer_sleep_ticks(uint64_t ticks);
void timer_sleep_seconds(uint32_t seconds);

}

using drivers::pit_init;
using drivers::pit_irq_handler;
using drivers::timer_get_ticks;
using drivers::pit_get_ticks;
using drivers::timer_get_uptime_seconds;
using drivers::timer_get_uptime;
using drivers::timer_sleep_ticks;
using drivers::timer_sleep_seconds;
