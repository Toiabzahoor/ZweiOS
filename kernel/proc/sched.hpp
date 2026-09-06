

#pragma once

#include <stdint.h>
#include <stddef.h>
#include "proc/process.hpp"
#include "arch/x86_64/isr.hpp"

namespace proc {

inline constexpr uint32_t SCHED_DEFAULT_QUANTUM = 5;

void sched_init();
void sched_tick(arch::cpu_registers_t* regs);
void sched_yield();
void sched_switch_to(PCB* next_proc);
void sched_reschedule();
PCB* sched_pick_next();

}

using proc::SCHED_DEFAULT_QUANTUM;
using proc::sched_init;
using proc::sched_tick;
using proc::sched_yield;
using proc::sched_switch_to;
using proc::sched_reschedule;
using proc::sched_pick_next;
