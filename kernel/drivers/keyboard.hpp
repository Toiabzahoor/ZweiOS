

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "arch/x86_64/isr.hpp"

namespace drivers {


inline constexpr size_t KEYBOARD_BUFFER_SIZE = 256;

void keyboard_init();
bool keyboard_has_char();
char keyboard_getchar();
bool keyboard_try_getchar(char* out);
void keyboard_poll();


void keyboard_irq_handler(arch::cpu_registers_t* regs);


bool sys_try_getc(char* out);

}

using drivers::keyboard_init;
using drivers::keyboard_has_char;
using drivers::keyboard_getchar;
using drivers::keyboard_try_getchar;
using drivers::sys_try_getc;
