# Project: ZweiOS Expansion & Branding

## Architecture
ZweiOS is a freestanding x86_64 operating system booted via Multiboot (ELF64 with 32-bit Multiboot trampoline).
- **Core Kernel**: `kernel/kmain.cpp`, `kernel/arch/x86_64/` (GDT, IDT, ISR, PIC, Port I/O, CPUID).
- **Drivers**: `kernel/drivers/` (VGA text mode, 16550 UART serial COM1, PS/2 Keyboard 8042, PIT 8254 Timer, CMOS RTC).
- **Memory Management**: `kernel/mm/` (PMM bitmap allocator, VMM 4-level paging with HHDM, Free-list Heap).
- **Library**: `kernel/lib/` (kprintf dual-stream mirror, string routines, kernel panic).
- **Shell**: `kernel/shell/` (Line buffer editor, token parser, command dispatcher, built-in commands).
- **Test Infrastructure**: `tests/` (`e2e_runner.py`, QEMU runner, Mock replay engine, test tiers 1-5).

## Feature Inventory
| # | Feature | Description | Milestone | Source |
|---|---------|-------------|-----------|--------|
| 1 | F1: Boot Banner Watermark | "made by toiabzahoor" emitted to VGA console & COM1 serial during early startup | M1 | ORIGINAL_REQUEST §R1 |
| 2 | F2: Shell Welcome Banner | "made by toiabzahoor" displayed above `zwei>` prompt on shell startup | M1 | ORIGINAL_REQUEST §R1 |
| 3 | F3: Shell Help Header | "made by toiabzahoor" in help command header | M1 | ORIGINAL_REQUEST §R1 |
| 4 | F4: Version / About Commands | `version` and `about` commands displaying OS version and author watermark | M1 | ORIGINAL_REQUEST §R1 |
| 5 | F5: CMOS RTC Driver | Hardware RTC read from ports 0x70/0x71, UIP wait, BCD/24h decoding, century logic | M2 | ORIGINAL_REQUEST §R3 |
| 6 | F6: System Uptime Subsystem | 8254 PIT 100Hz timer on IRQ0, monotonic tick and second tracking | M2 | ORIGINAL_REQUEST §R3 |
| 7 | F7: Shell `date` Command | Output formatted calendar date (YYYY-MM-DD) mirrored to VGA and COM1 | M2 | ORIGINAL_REQUEST §R3 |
| 8 | F8: Shell `time` Command | Output formatted wall-clock time (HH:MM:SS UTC) mirrored to VGA and COM1 | M2 | ORIGINAL_REQUEST §R3 |
| 9 | F9: Shell `uptime` Command | Output formatted uptime in hours, minutes, seconds mirrored to VGA and COM1 | M2 | ORIGINAL_REQUEST §R3 |
| 10 | F10: Shell `sleep` Command | `sleep <sec>` pauses shell execution and returns prompt cleanly | M2 | ORIGINAL_REQUEST §R3 |
| 11 | F11: Comment Stripping | Complete removal of `//`, `/* */`, and `;` comments across all `kernel/` files | M3 | ORIGINAL_REQUEST §R2 |
| 12 | F12: Test Harness & Mock Sync | Update `MockKernelReplayEngine` and test framework for new commands/branding | E2E | ORIGINAL_REQUEST §R4 |
| 13 | F13: Watermark Test Suite | Automated test cases verifying watermark presence across all interfaces | E2E | ORIGINAL_REQUEST §R4 |
| 14 | F14: Zero-Comment Scanner Test | Automated test scanning all files under `kernel/` for zero comment tokens | E2E | ORIGINAL_REQUEST §R4 |
| 15 | F15: RTC & Uptime Test Suite | Automated test cases for `date`, `time`, `uptime`, and `sleep` commands | E2E | ORIGINAL_REQUEST §R4 |
| 16 | F16: E2E Verification & Audit | 100% pass on all 74+ test cases and Forensic Integrity Audit verification | M4 (Final) | ORIGINAL_REQUEST §R4 |

## Milestones
| # | Name | Scope | Dependencies | Status |
|---|------|-------|-------------|--------|
| E2E | E2E Testing Suite & Infra | Test runner, mock engine updates, and test suites for R1, R2, R3, R4 | None | PLANNED |
| M1 | Author Branding & Watermark | F1, F2, F3, F4 in kmain, shell, commands | None | PLANNED |
| M2 | RTC & Uptime Subsystem | F5, F6, F7, F8, F9, F10 (drivers/rtc, drivers/pit, shell commands) | None | PLANNED |
| M3 | Codebase Comment Stripping | F11 across all files in kernel/ (.cpp, .hpp, .S) | M1, M2 | PLANNED |
| M4 | Final Milestone & Hardening | F16: 100% test pass on all suites + Tier 5 Adversarial Hardening + Audit | E2E, M1, M2, M3 | PLANNED |

## Interface Contracts

### 1. `drivers/rtc.hpp` Interface Contract
```cpp
#pragma once
#include <stdint.h>
#include <stddef.h>

namespace drivers {

struct rtc_time_t {
    uint8_t  second;
    uint8_t  minute;
    uint8_t  hour;
    uint8_t  day;
    uint8_t  month;
    uint32_t year;
};

void rtc_init();
rtc_time_t rtc_get_time();
void rtc_format_date(const rtc_time_t* t, char* buf, size_t buf_size);
void rtc_format_time(const rtc_time_t* t, char* buf, size_t buf_size);

} // namespace drivers
```

### 2. `drivers/pit.hpp` Interface Contract
```cpp
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
uint64_t timer_get_uptime_seconds();
void timer_get_uptime(uint32_t* hours, uint32_t* minutes, uint32_t* seconds);
void timer_sleep_ticks(uint64_t ticks);
void timer_sleep_seconds(uint32_t seconds);

} // namespace drivers
```

### 3. Shell Command Registry Contract (`kernel/shell/shell.hpp`)
- Command table capacity extended to 32 entries.
- Command signatures: `int cmd_name(int argc, char* argv[])`.
- Registered commands:
  - `help` (header updated with watermark)
  - `version` / `about` (watermark display)
  - `date` (formatted YYYY-MM-DD)
  - `time` (formatted HH:MM:SS UTC)
  - `uptime` (formatted uptime string)
  - `sleep` (sleep N seconds)

## Code Layout
```
kernel/
├── arch/x86_64/
│   ├── boot.S
│   ├── gdt_flush.S
│   ├── isr_stubs.S
│   ├── cpuid.hpp / cpuid.cpp
│   ├── gdt.hpp / gdt.cpp
│   ├── idt.hpp / idt.cpp
│   ├── io.hpp
│   ├── isr.hpp / isr.cpp
│   └── pic.hpp / pic.cpp
├── drivers/
│   ├── keyboard.hpp / keyboard.cpp
│   ├── serial.hpp / serial.cpp
│   ├── vga.hpp / vga.cpp
│   ├── pit.hpp / pit.cpp           (New)
│   └── rtc.hpp / rtc.cpp           (New)
├── mm/
│   ├── heap.hpp / heap.cpp
│   ├── pmm.hpp / pmm.cpp
│   └── vmm.hpp / vmm.cpp
├── lib/
│   ├── kprintf.hpp / kprintf.cpp
│   ├── panic.hpp / panic.cpp
│   └── string.hpp / string.cpp
├── shell/
│   ├── shell.hpp / shell.cpp
│   └── commands.cpp
└── kmain.cpp
tests/
├── e2e_runner.py
├── test_tier1_features.py
├── test_tier2_boundary.py
├── test_tier3_combinations.py
├── test_tier4_workloads.py
├── test_tier5_adversarial.py
├── test_comment_stripping.py       (New)
├── test_watermark_branding.py      (New)
└── test_rtc_uptime_commands.py     (New)
```
