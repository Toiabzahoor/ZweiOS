

#include "proc/sched.hpp"
#include "proc/process.hpp"
#include "mm/vmm.hpp"
#include "arch/x86_64/gdt.hpp"
#include "drivers/serial.hpp"

namespace proc {

static bool g_scheduler_active = false;

void sched_init() {
    g_scheduler_active = true;
    drivers::serial_puts("[SCHED] Preemptive round-robin scheduler initialized (Quantum: 50ms / 5 ticks)\r\n");
}

PCB* sched_pick_next() {
    PCB* cur = process_get_current();
    size_t start_idx = 0;


    PCB all[MAX_PROCESSES];
    size_t total = process_get_all(all, MAX_PROCESSES);

    if (cur) {
        for (size_t i = 0; i < total; ++i) {
            if (all[i].pid == cur->pid) {
                start_idx = (i + 1) % total;
                break;
            }
        }
    }


    for (size_t step = 0; step < total; ++step) {
        size_t idx = (start_idx + step) % total;
        PCB* candidate = process_get_by_pid(all[idx].pid);
        if (candidate && candidate->state == ProcessState::READY) {
            return candidate;
        }
    }


    PCB* shell_proc = process_get_by_pid(0);
    if (shell_proc && (shell_proc->state == ProcessState::READY || shell_proc->state == ProcessState::RUNNING)) {
        return shell_proc;
    }

    return nullptr;
}

void sched_tick(arch::cpu_registers_t* regs) {
    if (!g_scheduler_active || !regs) return;

    PCB* cur = process_get_current();
    if (!cur) return;

    cur->total_ticks++;

    if (cur->time_slice > 0) {
        cur->time_slice--;
    }

    if (cur->time_slice == 0) {
        cur->time_slice = SCHED_DEFAULT_QUANTUM;

        PCB* next = sched_pick_next();
        if (next && next != cur) {

            cur->context.rax = regs->rax;
            cur->context.rbx = regs->rbx;
            cur->context.rcx = regs->rcx;
            cur->context.rdx = regs->rdx;
            cur->context.rsi = regs->rsi;
            cur->context.rdi = regs->rdi;
            cur->context.rbp = regs->rbp;
            cur->context.r8  = regs->r8;
            cur->context.r9  = regs->r9;
            cur->context.r10 = regs->r10;
            cur->context.r11 = regs->r11;
            cur->context.r12 = regs->r12;
            cur->context.r13 = regs->r13;
            cur->context.r14 = regs->r14;
            cur->context.r15 = regs->r15;
            cur->context.rip = regs->rip;
            cur->context.cs  = regs->cs;
            cur->context.rflags = regs->rflags;
            cur->context.rsp = regs->rsp;
            cur->context.ss  = regs->ss;

            if (cur->state == ProcessState::RUNNING) {
                cur->state = ProcessState::READY;
            }


            next->state = ProcessState::RUNNING;
            next->time_slice = SCHED_DEFAULT_QUANTUM;
            process_set_current(next);


            if (next->cr3 != 0 && next->cr3 != cur->cr3) {
                mm::vmm_switch_pml4(next->cr3);
            }


            if (next->kernel_stack_top != 0) {
                arch::gdt_set_kernel_stack(next->kernel_stack_top);
            }


            regs->rax = next->context.rax;
            regs->rbx = next->context.rbx;
            regs->rcx = next->context.rcx;
            regs->rdx = next->context.rdx;
            regs->rsi = next->context.rsi;
            regs->rdi = next->context.rdi;
            regs->rbp = next->context.rbp;
            regs->r8  = next->context.r8;
            regs->r9  = next->context.r9;
            regs->r10 = next->context.r10;
            regs->r11 = next->context.r11;
            regs->r12 = next->context.r12;
            regs->r13 = next->context.r13;
            regs->r14 = next->context.r14;
            regs->r15 = next->context.r15;
            regs->rip = next->context.rip;
            regs->cs  = next->context.cs;
            regs->rflags = next->context.rflags;
            regs->rsp = next->context.rsp;
            regs->ss  = next->context.ss;
        }
    }
}

void sched_yield() {
    PCB* cur = process_get_current();
    if (cur) {
        cur->time_slice = 0;
    }
}

void sched_switch_to(PCB* next_proc) {
    if (!next_proc) return;
    process_set_current(next_proc);
    if (next_proc->cr3 != 0) {
        mm::vmm_switch_pml4(next_proc->cr3);
    }
    if (next_proc->kernel_stack_top != 0) {
        arch::gdt_set_kernel_stack(next_proc->kernel_stack_top);
    }
}

void sched_reschedule() {
    sched_yield();
}

}
