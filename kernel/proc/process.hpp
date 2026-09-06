#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

namespace proc {

inline constexpr size_t MAX_PROCESSES    = 32;
inline constexpr size_t PROCESS_NAME_LEN = 32;
inline constexpr size_t KERNEL_STACK_SZ  = 16384;
inline constexpr size_t USER_STACK_SZ    = 65536;

enum class ProcessState : uint8_t {
    UNUSED = 0,
    READY,
    RUNNING,
    BLOCKED,
    ZOMBIE
};

enum class ProcessType : uint8_t {
    KERNEL_THREAD = 0,
    LINUX_ELF64,
    WIN32_PE
};

struct [[gnu::packed]] CpuContext {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t rbp;
    uint64_t rbx;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rax;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;

    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

struct ProcessControlBlock {
    uint32_t     pid;
    char         name[PROCESS_NAME_LEN];
    ProcessState state;
    ProcessType  type;
    uint32_t     parent_pid;
    int64_t      exit_code;
    uint64_t     cr3;
    uint64_t     kernel_stack_base;
    uint64_t     kernel_stack_top;
    uint64_t     user_stack_base;
    uint64_t     user_stack_top;
    uint64_t     entry_point;
    uint64_t     sleep_until_tick;
    uint32_t     time_slice;
    uint32_t     total_ticks;

    CpuContext   context;
};

using PCB = ProcessControlBlock;

void process_init();
PCB* process_create(const char* name, uint64_t entry_point, ProcessType type, bool is_user_mode = false);
PCB* process_get_by_pid(uint32_t pid);
PCB* process_get_current();
void process_set_current(PCB* proc);
void process_exit(int64_t exit_code);
bool process_kill(uint32_t pid);
size_t process_get_all(PCB* out_list, size_t max_count);

int32_t process_fork();
int64_t process_execve(const char* path, const char* const argv[], const char* const envp[]);
int32_t process_waitpid(int32_t pid, int* wstatus, int options);

}

using proc::ProcessState;
using proc::ProcessType;
using proc::CpuContext;
using proc::ProcessControlBlock;
using proc::PCB;
using proc::process_init;
using proc::process_create;
using proc::process_get_by_pid;
using proc::process_get_current;
using proc::process_set_current;
using proc::process_exit;
using proc::process_kill;
using proc::process_get_all;
using proc::process_fork;
using proc::process_execve;
using proc::process_waitpid;
