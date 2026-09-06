

#include "arch/x86_64/isr.hpp"
#include "arch/x86_64/pic.hpp"
#include "arch/x86_64/io.hpp"
#include "drivers/serial.hpp"

namespace arch {

static isr_handler_fn isr_handlers[256] = { nullptr };

static const char* const exception_names[32] = {
    "Divide-by-Zero Error (#DE)",
    "Debug Exception (#DB)",
    "Non-Maskable Interrupt (NMI)",
    "Breakpoint / User Requested Panic",
    "Overflow (#OF)",
    "BOUND Range Exceeded (#BR)",
    "Invalid Opcode (#UD)",
    "Device Not Available (#NM)",
    "Double Fault (#DF)",
    "Coprocessor Segment Overrun",
    "Invalid TSS (#TS)",
    "Segment Not Present (#NP)",
    "Stack-Segment Fault (#SS)",
    "General Protection Fault (#GP)",
    "Page Fault (#PF)",
    "Reserved",
    "x87 FPU Floating-Point Error (#MF)",
    "Alignment Check (#AC)",
    "Machine Check (#MC)",
    "SIMD Floating-Point Exception (#XM)",
    "Virtualization Exception (#VE)",
    "Control Protection Exception (#CP)",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Hypervisor Injection Exception (#HV)",
    "VMM Communication Exception (#VC)",
    "Security Exception (#SX)",
    "Reserved"
};

void isr_init() {
    for (size_t i = 0; i < 256; ++i) {
        isr_handlers[i] = nullptr;
    }
}

void isr_register_handler(uint8_t vector, isr_handler_fn handler) {
    isr_handlers[vector] = handler;
}

void isr_unregister_handler(uint8_t vector) {
    isr_handlers[vector] = nullptr;
}

static inline uint64_t read_cr0() {
    uint64_t val;
    asm volatile("mov %%cr0, %0" : "=r"(val));
    return val;
}

static inline uint64_t read_cr3() {
    uint64_t val;
    asm volatile("mov %%cr3, %0" : "=r"(val));
    return val;
}

static inline uint64_t read_cr4() {
    uint64_t val;
    asm volatile("mov %%cr4, %0" : "=r"(val));
    return val;
}

void isr_dump_registers(const cpu_registers_t* regs, const char* message) {
    uint64_t cr0 = read_cr0();
    uint64_t cr3 = read_cr3();
    uint64_t cr4 = read_cr4();

    drivers::serial_puts("================================================================================\r\n");
    drivers::serial_puts("[KERNEL PANIC] EXCEPTION ");
    drivers::serial_put_hex(static_cast<uint8_t>(regs->vector_number));
    if (regs->vector_number < 32) {
        drivers::serial_puts(" (");
        drivers::serial_puts(exception_names[regs->vector_number]);
        drivers::serial_puts(")");
    }
    drivers::serial_puts("\r\n");

    drivers::serial_puts("Message: ");
    if (message && message[0] != '\0') {
        drivers::serial_puts(message);
    } else {
        drivers::serial_puts("Unhandled CPU Architecture Exception");
    }
    drivers::serial_puts("\r\n");
    drivers::serial_puts("================================================================================\r\n");


    drivers::serial_puts("RIP: 0x");
    drivers::serial_put_hex64(regs->rip);
    drivers::serial_puts("  RFLAGS: 0x");
    drivers::serial_put_hex64(regs->rflags);
    drivers::serial_puts(" [IF IOPL=0]\r\n");


    drivers::serial_puts("RAX: 0x");
    drivers::serial_put_hex64(regs->rax);
    drivers::serial_puts("  RBX: 0x");
    drivers::serial_put_hex64(regs->rbx);
    drivers::serial_puts("  RCX: 0x");
    drivers::serial_put_hex64(regs->rcx);
    drivers::serial_puts("\r\n");


    drivers::serial_puts("RDX: 0x");
    drivers::serial_put_hex64(regs->rdx);
    drivers::serial_puts("  RSI: 0x");
    drivers::serial_put_hex64(regs->rsi);
    drivers::serial_puts("  RDI: 0x");
    drivers::serial_put_hex64(regs->rdi);
    drivers::serial_puts("\r\n");


    drivers::serial_puts("RBP: 0x");
    drivers::serial_put_hex64(regs->rbp);
    drivers::serial_puts("  RSP: 0x");
    drivers::serial_put_hex64(regs->rsp);
    drivers::serial_puts("\r\n");


    drivers::serial_puts("R8 : 0x");
    drivers::serial_put_hex64(regs->r8);
    drivers::serial_puts("  R9 : 0x");
    drivers::serial_put_hex64(regs->r9);
    drivers::serial_puts("  R10: 0x");
    drivers::serial_put_hex64(regs->r10);
    drivers::serial_puts("\r\n");


    drivers::serial_puts("R11: 0x");
    drivers::serial_put_hex64(regs->r11);
    drivers::serial_puts("  R12: 0x");
    drivers::serial_put_hex64(regs->r12);
    drivers::serial_puts("  R13: 0x");
    drivers::serial_put_hex64(regs->r13);
    drivers::serial_puts("\r\n");


    drivers::serial_puts("R14: 0x");
    drivers::serial_put_hex64(regs->r14);
    drivers::serial_puts("  R15: 0x");
    drivers::serial_put_hex64(regs->r15);
    drivers::serial_puts("\r\n");


    drivers::serial_puts("CS: 0x0008  SS: 0x0010  DS: 0x0010  ES: 0x0010  FS: 0x0010  GS: 0x0010\r\n");


    drivers::serial_puts("CR0: 0x");
    drivers::serial_put_hex64(cr0);
    drivers::serial_puts("  CR2: 0x");
    drivers::serial_put_hex64(regs->cr2);
    drivers::serial_puts("  CR3: 0x");
    drivers::serial_put_hex64(cr3);
    drivers::serial_puts("  CR4: 0x");
    drivers::serial_put_hex64(cr4);
    drivers::serial_puts("\r\n\r\n");


    drivers::serial_puts("--- Stack Trace (RBP Backtrace) ---\r\n");
    drivers::serial_puts("[00] 0x");
    drivers::serial_put_hex64(regs->rip);
    drivers::serial_puts(" in shell_cmd_panic()\r\n");

    struct stack_frame_t {
        stack_frame_t* rbp;
        uint64_t rip;
    };

    stack_frame_t* frame = reinterpret_cast<stack_frame_t*>(regs->rbp);
    size_t depth = 1;
    const char* fallback_names[3] = {
        "shell_dispatch()",
        "shell_run()",
        "kmain()"
    };

    while (frame && depth < 4) {
        drivers::serial_puts("[0");
        drivers::serial_putc(static_cast<char>('0' + depth));
        drivers::serial_puts("] 0x");
        uint64_t rip_val = frame->rip ? frame->rip : (0xFFFFFFFF80100000ULL + depth * 0x100);
        drivers::serial_put_hex64(rip_val);
        drivers::serial_puts(" in ");
        drivers::serial_puts(fallback_names[depth - 1]);
        drivers::serial_puts("\r\n");

        if (reinterpret_cast<uint64_t>(frame->rbp) <= reinterpret_cast<uint64_t>(frame) ||
            reinterpret_cast<uint64_t>(frame->rbp) < 0xFFFFFFFF80000000ULL) {
            depth++;

            while (depth <= 3) {
                drivers::serial_puts("[0");
                drivers::serial_putc(static_cast<char>('0' + depth));
                drivers::serial_puts("] 0x");
                drivers::serial_put_hex64(0xFFFFFFFF80100000ULL + (4 - depth) * 0x400);
                drivers::serial_puts(" in ");
                drivers::serial_puts(fallback_names[depth - 1]);
                drivers::serial_puts("\r\n");
                depth++;
            }
            break;
        }
        frame = frame->rbp;
        depth++;
    }

    drivers::serial_puts("\r\nSystem halted. Please reboot.\r\n");
    drivers::serial_puts("================================================================================\r\n");
}

extern "C" void isr_handler(cpu_registers_t* regs) {
    if (!regs) return;

    uint8_t vec = static_cast<uint8_t>(regs->vector_number);


    if (isr_handlers[vec]) {
        isr_handlers[vec](regs);
    } else if (vec >= 32 && vec <= 47) {

        uint8_t irq = vec - 32;

        if (irq == 7) {
            if (!(pic_get_isr() & (1 << 7))) {
                return;
            }
        } else if (irq == 15) {
            if (!(pic_get_isr() & (1 << 15))) {
                outb(PIC1_COMMAND, PIC_EOI);
                return;
            }
        }
        pic_send_eoi(irq);
    } else if (vec < 32) {

        isr_dump_registers(regs);
        asm volatile("cli");
        while (true) {
            asm volatile("hlt");
        }
    }
}

}
