/* ==============================================================================
 * ZweiOS - Bare-Metal x86_64 Operating System
 * Component: PS/2 8042 Keyboard Controller & Scancode Decoder
 * ============================================================================== */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "arch/x86_64/isr.hpp"

namespace drivers {

// Keyboard Circular Ring Buffer Size
inline constexpr size_t KEYBOARD_BUFFER_SIZE = 256;

void keyboard_init();
bool keyboard_has_char();
char keyboard_getchar();
bool keyboard_try_getchar(char* out);
void keyboard_poll();

// IRQ1 Assembly ISR callback
void keyboard_irq_handler(arch::cpu_registers_t* regs);

// Dual-stream input multiplexer (pulls from keyboard buffer first, then serial COM1)
bool sys_try_getc(char* out);

} // namespace drivers

using drivers::keyboard_init;
using drivers::keyboard_has_char;
using drivers::keyboard_getchar;
using drivers::keyboard_try_getchar;
using drivers::sys_try_getc;
