

#pragma once

#include <stdint.h>
#include <stddef.h>

namespace arch {




struct [[gnu::packed]] cpu_registers_t {

    uint64_t cr2;
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;


    uint64_t vector_number;
    uint64_t error_code;


    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};
static_assert(sizeof(cpu_registers_t) == 184, "cpu_registers_t must be exactly 184 bytes");

typedef void (*isr_handler_fn)(cpu_registers_t* regs);


void isr_init();
void isr_register_handler(uint8_t vector, isr_handler_fn handler);
void isr_unregister_handler(uint8_t vector);
void isr_dump_registers(const cpu_registers_t* regs, const char* message = nullptr);


extern "C" void isr_handler(cpu_registers_t* regs);

}

using arch::cpu_registers_t;
using arch::isr_handler_fn;
using arch::isr_init;
using arch::isr_register_handler;
using arch::isr_unregister_handler;
using arch::isr_dump_registers;
