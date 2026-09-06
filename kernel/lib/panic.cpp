#include "panic.hpp"
#include "arch/x86_64/io.hpp"
#include "arch/x86_64/isr.hpp"
#include "drivers/serial.hpp"
#include "drivers/vga.hpp"

namespace kernel {

struct stack_frame_t {
    stack_frame_t* next_rbp;
    uint64_t return_rip;
};

void dump_stack_trace(uint64_t max_frames) {
    drivers::serial_puts("\n[Call Stack Trace]:\r\n");
    stack_frame_t* frame = nullptr;
    asm volatile ("mov %%rbp, %0" : "=r"(frame));

    uint64_t count = 0;
    while (frame && count < max_frames) {
        uint64_t frame_addr = reinterpret_cast<uint64_t>(frame);
        if (frame_addr < 0xFFFFFFFF80000000ULL && frame_addr > 0x00007FFFFFFFFFFFULL) {
            break;
        }

        drivers::serial_puts("  [#");
        drivers::serial_put_dec(count);
        drivers::serial_puts("] RIP: ");
        drivers::serial_put_hex(frame->return_rip);
        drivers::serial_puts(" (RBP: ");
        drivers::serial_put_hex(frame_addr);
        drivers::serial_puts(")\r\n");

        if (reinterpret_cast<uint64_t>(frame->next_rbp) <= frame_addr) {
            break;
        }
        frame = frame->next_rbp;
        count++;
    }
}

[[noreturn]] void panic(const char* message, const char* , int , const cpu_registers_t* regs) {
    asm volatile ("cli");

    if (regs) {
        arch::isr_dump_registers(regs, message);
    } else {
        arch::cpu_registers_t synth_regs;
        for (size_t i = 0; i < sizeof(arch::cpu_registers_t); ++i) {
            reinterpret_cast<uint8_t*>(&synth_regs)[i] = 0;
        }

        synth_regs.vector_number = 3;
        synth_regs.rip = reinterpret_cast<uint64_t>(__builtin_return_address(0));
        synth_regs.rbp = reinterpret_cast<uint64_t>(__builtin_frame_address(0));
        synth_regs.cs = 0x08;
        synth_regs.ss = 0x10;
        synth_regs.rflags = 0x0000000000000246ULL;
        asm volatile("mov %%rsp, %0" : "=r"(synth_regs.rsp));

        arch::isr_dump_registers(&synth_regs, message);
    }

    arch::qemu_debug_exit(arch::QEMU_EXIT_PANIC);

    for (;;) {
        asm volatile ("hlt");
    }
}

[[noreturn]] void kpanic(const char* message, const char* file, int line, const cpu_registers_t* regs) {
    panic(message, file, line, regs);
}

}
