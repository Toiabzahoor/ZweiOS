#include "proc/process.hpp"
#include "proc/sched.hpp"
#include "mm/vmm.hpp"
#include "mm/heap.hpp"
#include "arch/x86_64/gdt.hpp"
#include "lib/string.hpp"
#include "lib/kprintf.hpp"
#include "fs/vfs.hpp"
#include "loader/loader.hpp"

namespace proc {

static PCB g_process_table[MAX_PROCESSES];
static PCB* g_current_proc = nullptr;
static uint32_t g_next_pid = 1;

void process_init() {
    for (size_t i = 0; i < MAX_PROCESSES; ++i) {
        g_process_table[i].pid = 0;
        g_process_table[i].state = ProcessState::UNUSED;
        g_process_table[i].type = ProcessType::KERNEL_THREAD;
        g_process_table[i].parent_pid = 0;
        g_process_table[i].cr3 = 0;
        g_process_table[i].kernel_stack_base = 0;
        g_process_table[i].kernel_stack_top = 0;
        g_process_table[i].user_stack_base = 0;
        g_process_table[i].user_stack_top = 0;
        g_process_table[i].entry_point = 0;
        g_process_table[i].sleep_until_tick = 0;
        g_process_table[i].exit_code = 0;
        g_process_table[i].time_slice = SCHED_DEFAULT_QUANTUM;
        g_process_table[i].total_ticks = 0;
        lib::memset(g_process_table[i].name, 0, PROCESS_NAME_LEN);
        lib::memset(&g_process_table[i].context, 0, sizeof(CpuContext));
    }

    PCB* kernel_proc = &g_process_table[0];
    kernel_proc->pid = 0;
    lib::strncpy(kernel_proc->name, "kernel_shell", PROCESS_NAME_LEN - 1);
    kernel_proc->state = ProcessState::RUNNING;
    kernel_proc->type = ProcessType::KERNEL_THREAD;
    kernel_proc->cr3 = mm::vmm_get_kernel_pml4();
    kernel_proc->time_slice = SCHED_DEFAULT_QUANTUM;

    g_current_proc = kernel_proc;
}

PCB* process_create(const char* name, uint64_t entry_point, ProcessType type, bool is_user_mode) {
    size_t slot = MAX_PROCESSES;
    for (size_t i = 1; i < MAX_PROCESSES; ++i) {
        if (g_process_table[i].state == ProcessState::UNUSED || g_process_table[i].state == ProcessState::ZOMBIE) {
            slot = i;
            break;
        }
    }

    if (slot == MAX_PROCESSES) {
        return nullptr;
    }

    PCB* proc = &g_process_table[slot];
    proc->pid = g_next_pid++;
    lib::strncpy(proc->name, name ? name : "process", PROCESS_NAME_LEN - 1);
    proc->state = ProcessState::READY;
    proc->type = type;
    proc->parent_pid = g_current_proc ? g_current_proc->pid : 0;
    proc->entry_point = entry_point;
    proc->exit_code = 0;
    proc->time_slice = SCHED_DEFAULT_QUANTUM;
    proc->total_ticks = 0;
    proc->sleep_until_tick = 0;

    proc->cr3 = mm::vmm_create_user_pml4();
    if (proc->cr3 == 0) {
        proc->cr3 = mm::vmm_get_kernel_pml4();
    }

    if (proc->kernel_stack_base == 0) {
        proc->kernel_stack_base = reinterpret_cast<uint64_t>(mm::kmalloc(KERNEL_STACK_SZ));
    }
    if (proc->kernel_stack_base != 0) {
        proc->kernel_stack_top = (proc->kernel_stack_base + KERNEL_STACK_SZ) & ~0xFULL;
    }

    lib::memset(&proc->context, 0, sizeof(CpuContext));
    proc->context.rip = entry_point;
    proc->context.rflags = 0x202;

    if (is_user_mode) {
        proc->context.cs = arch::USER_CS;
        proc->context.ss = arch::USER_DS;
        proc->context.rsp = proc->user_stack_top;
    } else {
        proc->context.cs = arch::KERNEL_CS;
        proc->context.ss = arch::KERNEL_DS;
        proc->context.rsp = proc->kernel_stack_top;
    }

    return proc;
}

PCB* process_get_by_pid(uint32_t pid) {
    for (size_t i = 0; i < MAX_PROCESSES; ++i) {
        if (g_process_table[i].state != ProcessState::UNUSED && g_process_table[i].pid == pid) {
            return &g_process_table[i];
        }
    }
    return nullptr;
}

PCB* process_get_current() {
    return g_current_proc;
}

void process_set_current(PCB* proc) {
    g_current_proc = proc;
}

void process_exit(int64_t exit_code) {
    if (!g_current_proc) return;

    g_current_proc->state = ProcessState::ZOMBIE;
    g_current_proc->exit_code = exit_code;

    if (g_current_proc->cr3 != 0 && g_current_proc->cr3 != mm::vmm_get_kernel_pml4()) {
        mm::vmm_destroy_user_pml4(g_current_proc->cr3);
        g_current_proc->cr3 = mm::vmm_get_kernel_pml4();
    }

    sched_yield();
}

bool process_kill(uint32_t pid) {
    if (pid == 0) return false;

    PCB* proc = process_get_by_pid(pid);
    if (!proc || proc->state == ProcessState::UNUSED) {
        return false;
    }

    proc->state = ProcessState::ZOMBIE;
    proc->exit_code = -9;

    if (proc->cr3 != 0 && proc->cr3 != mm::vmm_get_kernel_pml4()) {
        mm::vmm_destroy_user_pml4(proc->cr3);
        proc->cr3 = mm::vmm_get_kernel_pml4();
    }

    return true;
}

size_t process_get_all(PCB* out_list, size_t max_count) {
    if (!out_list || max_count == 0) return 0;

    size_t count = 0;
    for (size_t i = 0; i < MAX_PROCESSES && count < max_count; ++i) {
        if (g_process_table[i].state != ProcessState::UNUSED) {
            out_list[count++] = g_process_table[i];
        }
    }
    return count;
}

int32_t process_fork() {
    size_t slot = MAX_PROCESSES;
    for (size_t i = 1; i < MAX_PROCESSES; ++i) {
        if (g_process_table[i].state == ProcessState::UNUSED || g_process_table[i].state == ProcessState::ZOMBIE) {
            slot = i;
            break;
        }
    }

    if (slot == MAX_PROCESSES) {
        return -11;
    }

    PCB* child = &g_process_table[slot];
    child->pid = g_next_pid++;
    child->parent_pid = g_current_proc ? g_current_proc->pid : 0;
    child->state = ProcessState::READY;
    child->type = g_current_proc ? g_current_proc->type : ProcessType::LINUX_ELF64;
    child->exit_code = 0;
    child->time_slice = SCHED_DEFAULT_QUANTUM;
    child->total_ticks = 0;
    child->sleep_until_tick = 0;
    lib::strncpy(child->name, g_current_proc ? g_current_proc->name : "fork_child", PROCESS_NAME_LEN - 1);

    child->cr3 = mm::vmm_create_user_pml4();
    if (child->cr3 == 0) {
        child->cr3 = mm::vmm_get_kernel_pml4();
    }

    if (child->kernel_stack_base == 0) {
        child->kernel_stack_base = reinterpret_cast<uint64_t>(mm::kmalloc(KERNEL_STACK_SZ));
    }
    if (child->kernel_stack_base != 0) {
        child->kernel_stack_top = (child->kernel_stack_base + KERNEL_STACK_SZ) & ~0xFULL;
    }

    if (g_current_proc) {
        child->context = g_current_proc->context;
        child->context.rax = 0;
        child->user_stack_base = g_current_proc->user_stack_base;
        child->user_stack_top = g_current_proc->user_stack_top;
        child->entry_point = g_current_proc->entry_point;
    }

    return static_cast<int32_t>(child->pid);
}

int32_t process_waitpid(int32_t pid, int* wstatus, int options) {
    uint32_t cur_pid = g_current_proc ? g_current_proc->pid : 0;

    while (true) {
        bool has_children = false;
        for (size_t i = 1; i < MAX_PROCESSES; ++i) {
            if (g_process_table[i].state == ProcessState::UNUSED) continue;
            if (g_process_table[i].parent_pid == cur_pid || cur_pid == 0) {
                if (pid == -1 || static_cast<int32_t>(g_process_table[i].pid) == pid) {
                    has_children = true;
                    if (g_process_table[i].state == ProcessState::ZOMBIE) {
                        int32_t child_pid = static_cast<int32_t>(g_process_table[i].pid);
                        if (wstatus) {
                            *wstatus = static_cast<int>((g_process_table[i].exit_code & 0xFF) << 8);
                        }
                        g_process_table[i].state = ProcessState::UNUSED;
                        g_process_table[i].pid = 0;
                        return child_pid;
                    }
                }
            }
        }

        if (!has_children) {
            return -10;
        }

        if (options & 1) {
            return 0;
        }

        sched_yield();
    }
}

int64_t process_execve(const char* path, const char* const argv[], const char* const envp[]) {
    (void)envp;
    if (!path) return -14;

    fs::VNodeStat st;
    if (fs::vfs_stat(path, &st) != 0 || st.type != fs::VNodeType::FILE) {
        return -2;
    }

    int argc = 0;
    if (argv) {
        while (argv[argc]) argc++;
    }

    const char* default_argv[] = { path, nullptr };
    const char* const* active_argv = (argv && argc > 0) ? argv : default_argv;
    int active_argc = (argc > 0) ? argc : 1;

    if (g_current_proc) {
        const char* slash = nullptr;
        for (const char* p = path; *p; ++p) {
            if (*p == '/' || *p == '\\') slash = p + 1;
        }
        lib::strncpy(g_current_proc->name, slash ? slash : path, PROCESS_NAME_LEN - 1);
    }

    for (int i = 3; i < static_cast<int>(fs::MAX_OPEN_FILES); ++i) {
        fs::FileDescriptor* fd_desc = fs::vfs_get_fd(i);
        if (fd_desc && (fd_desc->flags & fs::O_CLOEXEC)) {
            fs::vfs_close(i);
        }
    }

    int64_t code = loader::loader_execute_path(path, active_argc, active_argv);
    process_exit(code);
    return code;
}

}
