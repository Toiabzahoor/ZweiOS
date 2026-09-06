#pragma once

#include <stdint.h>
#include <stddef.h>

namespace syscall {

inline constexpr uint64_t SYS_READ         = 0;
inline constexpr uint64_t SYS_WRITE        = 1;
inline constexpr uint64_t SYS_OPEN         = 2;
inline constexpr uint64_t SYS_CLOSE        = 3;
inline constexpr uint64_t SYS_STAT         = 4;
inline constexpr uint64_t SYS_FSTAT        = 5;
inline constexpr uint64_t SYS_POLL         = 7;
inline constexpr uint64_t SYS_LSEEK        = 8;
inline constexpr uint64_t SYS_MMAP         = 9;
inline constexpr uint64_t SYS_MPROTECT     = 10;
inline constexpr uint64_t SYS_MUNMAP       = 11;
inline constexpr uint64_t SYS_BRK          = 12;
inline constexpr uint64_t SYS_RT_SIGACTION = 13;
inline constexpr uint64_t SYS_RT_SIGPROCMASK= 14;
inline constexpr uint64_t SYS_IOCTL        = 16;
inline constexpr uint64_t SYS_READV        = 19;
inline constexpr uint64_t SYS_WRITEV       = 20;
inline constexpr uint64_t SYS_ACCESS       = 21;
inline constexpr uint64_t SYS_PIPE         = 22;
inline constexpr uint64_t SYS_SELECT       = 23;
inline constexpr uint64_t SYS_SCHED_YIELD  = 24;
inline constexpr uint64_t SYS_DUP          = 32;
inline constexpr uint64_t SYS_DUP2         = 33;
inline constexpr uint64_t SYS_NANOSLEEP    = 35;
inline constexpr uint64_t SYS_GETPID       = 39;
inline constexpr uint64_t SYS_SOCKET       = 41;
inline constexpr uint64_t SYS_CONNECT      = 42;
inline constexpr uint64_t SYS_ACCEPT       = 43;
inline constexpr uint64_t SYS_SENDTO       = 44;
inline constexpr uint64_t SYS_RECVFROM     = 45;
inline constexpr uint64_t SYS_SENDMSG      = 46;
inline constexpr uint64_t SYS_RECVMSG      = 47;
inline constexpr uint64_t SYS_SHUTDOWN     = 48;
inline constexpr uint64_t SYS_BIND         = 49;
inline constexpr uint64_t SYS_LISTEN       = 50;
inline constexpr uint64_t SYS_GETSOCKNAME  = 51;
inline constexpr uint64_t SYS_GETPEERNAME  = 52;
inline constexpr uint64_t SYS_SOCKETPAIR   = 53;
inline constexpr uint64_t SYS_SETSOCKOPT   = 54;
inline constexpr uint64_t SYS_GETSOCKOPT   = 55;
inline constexpr uint64_t SYS_FORK         = 57;
inline constexpr uint64_t SYS_EXECVE       = 59;
inline constexpr uint64_t SYS_EXIT         = 60;
inline constexpr uint64_t SYS_WAIT4        = 61;
inline constexpr uint64_t SYS_UNAME        = 63;
inline constexpr uint64_t SYS_FCNTL        = 72;
inline constexpr uint64_t SYS_GETCWD       = 79;
inline constexpr uint64_t SYS_CHDIR        = 80;
inline constexpr uint64_t SYS_RENAME       = 82;
inline constexpr uint64_t SYS_MKDIR        = 83;
inline constexpr uint64_t SYS_RMDIR        = 84;
inline constexpr uint64_t SYS_UNLINK       = 87;
inline constexpr uint64_t SYS_GETRLIMIT    = 97;
inline constexpr uint64_t SYS_GETUID       = 102;
inline constexpr uint64_t SYS_GETGID       = 104;
inline constexpr uint64_t SYS_GETEUID      = 107;
inline constexpr uint64_t SYS_GETEGID      = 108;
inline constexpr uint64_t SYS_ARCH_PRCTL   = 158;
inline constexpr uint64_t SYS_SET_TID_ADDRESS = 218;
inline constexpr uint64_t SYS_CLOCK_GETTIME= 228;
inline constexpr uint64_t SYS_EXIT_GROUP   = 231;
inline constexpr uint64_t SYS_OPENAT       = 257;
inline constexpr uint64_t SYS_NEWFSTATAT   = 262;
inline constexpr uint64_t SYS_FACCESSAT    = 269;
inline constexpr uint64_t SYS_PIPE2        = 293;
inline constexpr uint64_t SYS_PRLIMIT64    = 302;

inline constexpr int64_t  LINUX_AT_FDCWD   = -100;

inline constexpr uint64_t ARCH_SET_GS      = 0x1001;
inline constexpr uint64_t ARCH_SET_FS      = 0x1002;
inline constexpr uint64_t ARCH_GET_FS      = 0x1003;
inline constexpr uint64_t ARCH_GET_GS      = 0x1004;

struct rlimit64 {
    uint64_t rlim_cur;
    uint64_t rlim_max;
};

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

struct linux_stat {
    uint64_t st_dev;
    uint64_t st_ino;
    uint64_t st_nlink;
    uint32_t st_mode;
    uint32_t st_uid;
    uint32_t st_gid;
    uint32_t __pad0;
    uint64_t st_rdev;
    int64_t  st_size;
    int64_t  st_blksize;
    int64_t  st_blocks;
    int64_t  st_atime_sec;
    uint64_t st_atime_nsec;
    int64_t  st_mtime_sec;
    uint64_t st_mtime_nsec;
    int64_t  st_ctime_sec;
    uint64_t st_ctime_nsec;
    int64_t  __unused[3];
};

void set_process_brk(uint64_t initial_brk);
void syscall_init();
int64_t run_user_test();

extern "C" void syscall_entry();
extern "C" int64_t enter_user_mode(uint64_t user_rip, uint64_t user_rsp);
extern "C" int64_t syscall_dispatch(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);

}

using syscall::syscall_init;
using syscall::run_user_test;
