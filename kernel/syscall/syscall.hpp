/* ==============================================================================
 * ZweiOS - Bare-Metal x86_64 Operating System
 * Component: Fast System Call Subsystem (IA32_LSTAR / syscall / sysretq) Header
 * ============================================================================== */

#pragma once

#include <stdint.h>
#include <stddef.h>

namespace syscall {

// ==============================================================================
// Standard Linux x86_64 Syscall Numbers
// ==============================================================================
inline constexpr uint64_t SYS_READ         = 0;
inline constexpr uint64_t SYS_WRITE        = 1;
inline constexpr uint64_t SYS_MMAP         = 9;
inline constexpr uint64_t SYS_MUNMAP       = 11;
inline constexpr uint64_t SYS_BRK          = 12;
inline constexpr uint64_t SYS_IOCTL        = 16;
inline constexpr uint64_t SYS_WRITEV       = 20;
inline constexpr uint64_t SYS_GETPID       = 39;
inline constexpr uint64_t SYS_EXIT         = 60;
inline constexpr uint64_t SYS_UNAME        = 63;
inline constexpr uint64_t SYS_GETUID       = 102;
inline constexpr uint64_t SYS_GETGID       = 104;
inline constexpr uint64_t SYS_GETEUID      = 107;
inline constexpr uint64_t SYS_GETEGID      = 108;
inline constexpr uint64_t SYS_ARCH_PRCTL   = 158;
inline constexpr uint64_t SYS_CLOCK_GETTIME= 228;
inline constexpr uint64_t SYS_EXIT_GROUP   = 231;

// Subfunctions for arch_prctl (158)
inline constexpr uint64_t ARCH_SET_GS      = 0x1001;
inline constexpr uint64_t ARCH_SET_FS      = 0x1002;
inline constexpr uint64_t ARCH_GET_FS      = 0x1003;
inline constexpr uint64_t ARCH_GET_GS      = 0x1004;

// POSIX structures for Linux syscalls
struct iovec {
    void*  iov_base;
    size_t iov_len;
};

struct utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};

struct timespec {
    int64_t tv_sec;
    int64_t tv_nsec;
};

void set_process_brk(uint64_t initial_brk);

// Initialize hardware MSRs (IA32_EFER.SCE, IA32_STAR, IA32_LSTAR, IA32_FMASK)
void syscall_init();

// Standalone user-mode test runner
int64_t run_user_test();

// Assembly entry points and stubs
extern "C" void syscall_entry();
extern "C" int64_t enter_user_mode(uint64_t user_rip, uint64_t user_rsp);
extern "C" int64_t syscall_dispatch(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);

} // namespace syscall

using syscall::syscall_init;
using syscall::run_user_test;
