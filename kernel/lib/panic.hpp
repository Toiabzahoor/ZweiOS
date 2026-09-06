#pragma once
#include <stdint.h>
#include <stddef.h>

namespace arch {
struct cpu_registers_t;
}

namespace kernel {


[[noreturn]] void panic(const char* message, const char* file, int line, const arch::cpu_registers_t* regs = nullptr);
[[noreturn]] void kpanic(const char* message, const char* file, int line, const arch::cpu_registers_t* regs = nullptr);


void dump_stack_trace(uint64_t max_frames = 10);

}


using kernel::panic;
using kernel::kpanic;
using kernel::dump_stack_trace;


#define KASSERT(cond, msg) \
    do { \
        if (__builtin_expect(!(cond), 0)) { \
            ::kernel::panic((msg), __FILE__, __LINE__, nullptr); \
        } \
    } while (0)
