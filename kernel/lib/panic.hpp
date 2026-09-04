#pragma once
#include <stdint.h>
#include <stddef.h>

namespace arch {
struct cpu_registers_t;
}

namespace kernel {

// Freestanding Kernel Panic Function
[[noreturn]] void panic(const char* message, const char* file, int line, const arch::cpu_registers_t* regs = nullptr);
[[noreturn]] void kpanic(const char* message, const char* file, int line, const arch::cpu_registers_t* regs = nullptr);

// Unwind Call Stack using RBP Frame Pointer Chain
void dump_stack_trace(uint64_t max_frames = 10);

} // namespace kernel

// Global aliases
using kernel::panic;
using kernel::kpanic;
using kernel::dump_stack_trace;

// Freestanding Kernel Assertion Macro
#define KASSERT(cond, msg) \
    do { \
        if (__builtin_expect(!(cond), 0)) { \
            ::kernel::panic((msg), __FILE__, __LINE__, nullptr); \
        } \
    } while (0)
