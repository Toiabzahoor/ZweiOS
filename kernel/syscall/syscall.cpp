#include "syscall/syscall.hpp"
#include "arch/x86_64/io.hpp"
#include "drivers/serial.hpp"
#include "drivers/keyboard.hpp"
#include "drivers/pit.hpp"
#include "mm/pmm.hpp"
#include "mm/vmm.hpp"
#include "lib/kprintf.hpp"
#include "lib/string.hpp"
#include "proc/process.hpp"
#include "proc/sched.hpp"
#include "fs/vfs.hpp"
#include "net/socket.hpp"

extern "C" uint64_t syscall_exit_requested;
extern "C" uint64_t syscall_exit_code;

static uint64_t current_process_brk = 0x0000000000600000ULL;
static uint64_t current_mmap_vaddr   = 0x0000000010000000ULL;

namespace syscall {

static void populate_linux_stat(const fs::VNodeStat& src, linux_stat* dst) {
    if (!dst) return;
    lib::memset(dst, 0, sizeof(linux_stat));
    dst->st_dev = 1;
    dst->st_ino = 100;
    dst->st_nlink = 1;
    dst->st_size = static_cast<int64_t>(src.size);
    dst->st_blksize = 4096;
    dst->st_blocks = (src.size + 511) / 512;
    dst->st_uid = 1000;
    dst->st_gid = 1000;

    if (src.type == fs::VNodeType::DIRECTORY) {
        dst->st_mode = 0040755;
    } else if (src.type == fs::VNodeType::PIPE) {
        dst->st_mode = 0010644;
    } else if (src.type == fs::VNodeType::DEVICE) {
        dst->st_mode = 0020666;
    } else {
        dst->st_mode = 0100755;
    }
}

void syscall_init() {
    uint64_t efer = arch::rdmsr(arch::IA32_EFER);
    arch::wrmsr(arch::IA32_EFER, efer | 1ULL);

    uint64_t star = (static_cast<uint64_t>(0x0010ULL) << 48) | (static_cast<uint64_t>(0x0008ULL) << 32);
    arch::wrmsr(arch::IA32_STAR, star);

    arch::wrmsr(arch::IA32_LSTAR, reinterpret_cast<uint64_t>(&syscall_entry));

    arch::wrmsr(arch::IA32_FMASK, 0x00000700ULL);

    drivers::serial_puts("[SYSCALL] Fast System Call (IA32_LSTAR) initialized. User Code 0x23, Data 0x1B\r\n");
}

void set_process_brk(uint64_t initial_brk) {
    current_process_brk = initial_brk;
}

extern "C" int64_t syscall_dispatch(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a4;
    (void)a5;
    (void)a6;

    switch (num) {
        case SYS_WRITE: {
            int fd = static_cast<int>(a1);
            const char* buf = reinterpret_cast<const char*>(a2);
            size_t count = static_cast<size_t>(a3);
            if (!buf || count == 0) return 0;

            if (net::sock_is_socket(fd)) {
                int64_t sent = net::sock_send(fd, buf, count, 0);
                return (sent >= 0) ? sent : -9;
            }

            if (fd == 1 || fd == 2) {
                fs::FileDescriptor* desc = fs::vfs_get_fd(fd);
                if (desc && desc->node != nullptr) {
                    return fs::vfs_write(fd, buf, count);
                }
                for (size_t i = 0; i < count; ++i) {
                    lib::kprint_char(buf[i]);
                }
                return static_cast<int64_t>(count);
            }
            int64_t bytes = fs::vfs_write(fd, buf, count);
            return (bytes >= 0) ? bytes : -9;
        }

        case SYS_EXIT:
        case SYS_EXIT_GROUP: {
            syscall_exit_requested = 1;
            syscall_exit_code = static_cast<uint64_t>(a1);
            proc::PCB* cur = proc::process_get_current();
            if (cur && cur->pid != 0) {
                cur->state = proc::ProcessState::ZOMBIE;
                cur->exit_code = static_cast<int64_t>(a1);
            }
            return 0;
        }

        case SYS_GETPID: {
            proc::PCB* cur = proc::process_get_current();
            return cur ? static_cast<int64_t>(cur->pid) : 1;
        }

        case SYS_SCHED_YIELD: {
            proc::sched_yield();
            return 0;
        }

        case SYS_READ: {
            int fd = static_cast<int>(a1);
            char* buf = reinterpret_cast<char*>(a2);
            size_t count = static_cast<size_t>(a3);
            if (!buf || count == 0) return 0;

            if (net::sock_is_socket(fd)) {
                int64_t recvd = net::sock_recv(fd, buf, count, 0);
                return (recvd >= 0) ? recvd : -9;
            }

            int64_t bytes = fs::vfs_read(fd, buf, count);
            return (bytes >= 0) ? bytes : -9;
        }

        case SYS_OPEN: {
            const char* path = reinterpret_cast<const char*>(a1);
            int flags = static_cast<int>(a2);
            if (!path) return -14;
            int fd = fs::vfs_open(path, flags);
            return (fd >= 0) ? fd : -2;
        }

        case SYS_CLOSE: {
            int fd = static_cast<int>(a1);
            if (net::sock_is_socket(fd)) {
                return (net::sock_close(fd) == 0) ? 0 : -9;
            }
            return (fs::vfs_close(fd) == 0) ? 0 : -9;
        }

        case SYS_STAT: {
            const char* path = reinterpret_cast<const char*>(a1);
            auto* st_dst = reinterpret_cast<linux_stat*>(a2);
            if (!path || !st_dst) return -14;
            fs::VNodeStat vst;
            if (fs::vfs_stat(path, &vst) != 0) return -2;
            populate_linux_stat(vst, st_dst);
            return 0;
        }

        case SYS_FSTAT: {
            int fd = static_cast<int>(a1);
            auto* st_dst = reinterpret_cast<linux_stat*>(a2);
            if (!st_dst) return -14;
            fs::VNodeStat vst;
            if (fs::vfs_fstat(fd, &vst) != 0) return -9;
            populate_linux_stat(vst, st_dst);
            return 0;
        }

        case SYS_POLL: {
            auto* fds = reinterpret_cast<net::pollfd*>(a1);
            uint64_t nfds = a2;
            int timeout = static_cast<int>(a3);
            (void)timeout;
            if (!fds || nfds == 0) return 0;
            int ready = 0;
            for (uint64_t i = 0; i < nfds; ++i) {
                fds[i].revents = 0;
                if (net::sock_is_socket(fds[i].fd)) {
                    if ((fds[i].events & net::POLLIN) && net::sock_poll_ready(fds[i].fd, false)) {
                        fds[i].revents |= net::POLLIN;
                        ready++;
                    }
                    if ((fds[i].events & net::POLLOUT) && net::sock_poll_ready(fds[i].fd, true)) {
                        fds[i].revents |= net::POLLOUT;
                        ready++;
                    }
                } else if (fds[i].fd >= 0) {
                    fds[i].revents = fds[i].events;
                    ready++;
                }
            }
            return ready;
        }

        case SYS_LSEEK: {
            int fd = static_cast<int>(a1);
            int64_t offset = static_cast<int64_t>(a2);
            int whence = static_cast<int>(a3);
            int64_t res = fs::vfs_lseek(fd, offset, whence);
            return (res >= 0) ? res : -9;
        }

        case SYS_MMAP: {
            uint64_t addr = a1;
            uint64_t length = a2;
            if (length == 0) return -22;
            uint64_t target_vaddr = addr;
            if (target_vaddr == 0) {
                target_vaddr = current_mmap_vaddr;
                size_t aligned_len = (length + 4095) & ~4095ULL;
                current_mmap_vaddr += aligned_len;
            }
            size_t pages = (length + 4095) / mm::PAGE_SIZE;
            for (size_t p = 0; p < pages; ++p) {
                uint64_t v = target_vaddr + (p * mm::PAGE_SIZE);
                uint64_t phys = mm::vmm_virt_to_phys(v);
                mm::vmm_map_page(v, phys, mm::PTE_PRESENT | mm::PTE_WRITABLE | mm::PTE_USER);
            }
            lib::memset(reinterpret_cast<void*>(target_vaddr), 0, length);
            return static_cast<int64_t>(target_vaddr);
        }

        case SYS_MPROTECT: {
            uint64_t addr = a1;
            uint64_t len = a2;
            uint64_t prot = a3;
            uint64_t flags = mm::PTE_PRESENT | mm::PTE_USER;
            if (prot & 2) flags |= mm::PTE_WRITABLE;
            if (!(prot & 4)) flags |= mm::PTE_NX;
            for (uint64_t v = addr; v < addr + len; v += mm::PAGE_SIZE) {
                uint64_t phys = mm::vmm_virt_to_phys(v);
                mm::vmm_map_page(v, phys, flags);
            }
            return 0;
        }

        case SYS_MUNMAP: {
            return 0;
        }

        case SYS_BRK: {
            uint64_t new_brk = a1;
            if (new_brk == 0) {
                return static_cast<int64_t>(current_process_brk);
            }
            if (new_brk > current_process_brk) {
                uint64_t start_page = (current_process_brk + 4095) & ~4095ULL;
                uint64_t end_page = (new_brk + 4095) & ~4095ULL;
                for (uint64_t v = start_page; v < end_page; v += mm::PAGE_SIZE) {
                    uint64_t p = mm::vmm_virt_to_phys(v);
                    mm::vmm_map_page(v, p, mm::PTE_PRESENT | mm::PTE_WRITABLE | mm::PTE_USER);
                }
            }
            current_process_brk = new_brk;
            return static_cast<int64_t>(current_process_brk);
        }

        case SYS_ACCESS: {
            const char* path = reinterpret_cast<const char*>(a1);
            int mode = static_cast<int>(a2);
            return (fs::vfs_access(path, mode) == 0) ? 0 : -2;
        }

        case SYS_PIPE: {
            int* pipefd = reinterpret_cast<int*>(a1);
            if (!pipefd) return -14;
            return fs::vfs_pipe(pipefd, 0);
        }

        case SYS_PIPE2: {
            int* pipefd = reinterpret_cast<int*>(a1);
            int flags = static_cast<int>(a2);
            if (!pipefd) return -14;
            return fs::vfs_pipe(pipefd, flags);
        }

        case SYS_DUP: {
            int oldfd = static_cast<int>(a1);
            return fs::vfs_dup(oldfd);
        }

        case SYS_DUP2: {
            int oldfd = static_cast<int>(a1);
            int newfd = static_cast<int>(a2);
            return fs::vfs_dup2(oldfd, newfd);
        }

        case SYS_FORK: {
            return proc::process_fork();
        }

        case SYS_EXECVE: {
            const char* path = reinterpret_cast<const char*>(a1);
            const char* const* argv = reinterpret_cast<const char* const*>(a2);
            const char* const* envp = reinterpret_cast<const char* const*>(a3);
            return proc::process_execve(path, argv, envp);
        }

        case SYS_WAIT4: {
            int32_t pid = static_cast<int32_t>(a1);
            int* wstatus = reinterpret_cast<int*>(a2);
            int options = static_cast<int>(a3);
            return proc::process_waitpid(pid, wstatus, options);
        }

        case SYS_FCNTL: {
            int fd = static_cast<int>(a1);
            int cmd = static_cast<int>(a2);
            uint64_t arg = a3;
            return fs::vfs_fcntl(fd, cmd, arg);
        }

        case SYS_GETCWD: {
            char* buf = reinterpret_cast<char*>(a1);
            size_t size = static_cast<size_t>(a2);
            if (!buf || size == 0) return -14;
            if (fs::vfs_getcwd(buf, size) != 0) return -34;
            return reinterpret_cast<int64_t>(buf);
        }

        case SYS_CHDIR: {
            const char* path = reinterpret_cast<const char*>(a1);
            if (!path) return -14;
            return (fs::vfs_chdir(path) == 0) ? 0 : -2;
        }

        case SYS_RENAME: {
            const char* oldpath = reinterpret_cast<const char*>(a1);
            const char* newpath = reinterpret_cast<const char*>(a2);
            if (!oldpath || !newpath) return -14;
            return (fs::vfs_rename(oldpath, newpath) == 0) ? 0 : -2;
        }

        case SYS_MKDIR: {
            const char* path = reinterpret_cast<const char*>(a1);
            if (!path) return -14;
            return (fs::vfs_mkdir(path) == 0) ? 0 : -1;
        }

        case SYS_RMDIR:
        case SYS_UNLINK: {
            const char* path = reinterpret_cast<const char*>(a1);
            if (!path) return -14;
            return (fs::vfs_unlink(path) == 0) ? 0 : -2;
        }

        case SYS_ARCH_PRCTL: {
            uint64_t code = a1;
            uint64_t addr = a2;
            if (code == ARCH_SET_FS) {
                arch::wrmsr(arch::IA32_FS_BASE, addr);
                return 0;
            } else if (code == ARCH_GET_FS) {
                if (!addr) return -14;
                *reinterpret_cast<uint64_t*>(addr) = arch::rdmsr(arch::IA32_FS_BASE);
                return 0;
            } else if (code == ARCH_SET_GS) {
                arch::wrmsr(arch::IA32_GS_BASE, addr);
                return 0;
            } else if (code == ARCH_GET_GS) {
                if (!addr) return -14;
                *reinterpret_cast<uint64_t*>(addr) = arch::rdmsr(arch::IA32_GS_BASE);
                return 0;
            }
            return -22;
        }

        case SYS_RT_SIGACTION:
        case SYS_RT_SIGPROCMASK: {
            return 0;
        }

        case SYS_READV: {
            int fd = static_cast<int>(a1);
            auto* iov = reinterpret_cast<iovec*>(a2);
            size_t iovcnt = static_cast<size_t>(a3);
            if (!iov || iovcnt == 0) return 0;

            size_t total_read = 0;
            for (size_t i = 0; i < iovcnt; ++i) {
                char* buf = reinterpret_cast<char*>(iov[i].iov_base);
                size_t count = iov[i].iov_len;
                if (buf && count > 0) {
                    int64_t res = fs::vfs_read(fd, buf, count);
                    if (res > 0) {
                        total_read += static_cast<size_t>(res);
                    } else if (res < 0) {
                        return (total_read > 0) ? static_cast<int64_t>(total_read) : res;
                    } else {
                        break;
                    }
                }
            }
            return static_cast<int64_t>(total_read);
        }

        case SYS_WRITEV: {
            int fd = static_cast<int>(a1);
            const auto* iov = reinterpret_cast<const iovec*>(a2);
            size_t iovcnt = static_cast<size_t>(a3);
            if (!iov || iovcnt == 0) return 0;

            size_t total_written = 0;
            for (size_t i = 0; i < iovcnt; ++i) {
                const char* buf = reinterpret_cast<const char*>(iov[i].iov_base);
                size_t count = iov[i].iov_len;
                if (buf && count > 0) {
                    int64_t res = fs::vfs_write(fd, buf, count);
                    if (res > 0) {
                        total_written += static_cast<size_t>(res);
                    }
                }
            }
            return static_cast<int64_t>(total_written);
        }

        case SYS_GETRLIMIT: {
            auto* rlim = reinterpret_cast<rlimit64*>(a2);
            if (!rlim) return -14;
            rlim->rlim_cur = 8388608ULL;
            rlim->rlim_max = 8388608ULL;
            return 0;
        }

        case SYS_PRLIMIT64: {
            auto* old_rlim = reinterpret_cast<rlimit64*>(a4);
            if (old_rlim) {
                old_rlim->rlim_cur = 8388608ULL;
                old_rlim->rlim_max = 8388608ULL;
            }
            return 0;
        }

        case SYS_SET_TID_ADDRESS: {
            proc::PCB* cur = proc::process_get_current();
            return cur ? static_cast<int64_t>(cur->pid) : 1;
        }

        case SYS_OPENAT: {
            const char* path = reinterpret_cast<const char*>(a2);
            int flags = static_cast<int>(a3);
            if (!path) return -14;
            return fs::vfs_open(path, flags);
        }

        case SYS_NEWFSTATAT: {
            const char* path = reinterpret_cast<const char*>(a2);
            auto* st_out = reinterpret_cast<linux_stat*>(a3);
            if (!path || !st_out) return -14;
            fs::VNodeStat st;
            if (fs::vfs_stat(path, &st) != 0) return -2;
            populate_linux_stat(st, st_out);
            return 0;
        }

        case SYS_FACCESSAT: {
            const char* path = reinterpret_cast<const char*>(a2);
            int mode = static_cast<int>(a3);
            if (!path) return -14;
            return (fs::vfs_access(path, mode) == 0) ? 0 : -2;
        }

        case SYS_UNAME: {
            auto* un = reinterpret_cast<utsname*>(a1);
            if (!un) return -14;
            lib::strncpy(un->sysname, "ZweiOS", sizeof(un->sysname));
            lib::strncpy(un->nodename, "zweios", sizeof(un->nodename));
            lib::strncpy(un->release, "1.0.0-dual", sizeof(un->release));
            lib::strncpy(un->version, "ZweiOS Native Linux ABI Subsystem", sizeof(un->version));
            lib::strncpy(un->machine, "x86_64", sizeof(un->machine));
            lib::strncpy(un->domainname, "(none)", sizeof(un->domainname));
            return 0;
        }

        case SYS_GETUID:
        case SYS_GETGID:
        case SYS_GETEUID:
        case SYS_GETEGID: {
            return 1000;
        }

        case SYS_IOCTL: {
            uint64_t req = a2;
            if (req == 0x5413) {
                struct winsize {
                    uint16_t ws_row;
                    uint16_t ws_col;
                    uint16_t ws_xpixel;
                    uint16_t ws_ypixel;
                };
                auto* ws = reinterpret_cast<winsize*>(a3);
                if (ws) {
                    ws->ws_col = 128;
                    ws->ws_row = 48;
                    ws->ws_xpixel = 1024;
                    ws->ws_ypixel = 768;
                }
            }
            return 0;
        }

        case SYS_CLOCK_GETTIME: {
            auto* ts = reinterpret_cast<timespec*>(a2);
            if (!ts) return -14;
            ts->tv_sec = static_cast<int64_t>(drivers::timer_get_uptime_seconds());
            ts->tv_nsec = static_cast<int64_t>((drivers::timer_get_ticks() % 100) * 10000000ULL);
            return 0;
        }

        case SYS_NANOSLEEP: {
            const auto* req = reinterpret_cast<const timespec*>(a1);
            if (!req) return -14;
            if (req->tv_sec > 0) {
                drivers::timer_sleep_seconds(static_cast<uint32_t>(req->tv_sec));
            }
            return 0;
        }

        case SYS_SOCKET: {
            int domain = static_cast<int>(a1);
            int type = static_cast<int>(a2) & 0xFF;
            int protocol = static_cast<int>(a3);
            int fd = net::sock_create(domain, type, protocol);
            return (fd >= 0) ? fd : -22;
        }

        case SYS_CONNECT: {
            int fd = static_cast<int>(a1);
            const auto* addr = reinterpret_cast<const net::sockaddr_in*>(a2);
            if (!addr) return -14;
            int res = net::sock_connect(fd, addr);
            return (res == 0) ? 0 : -111;
        }

        case SYS_ACCEPT: {
            int fd = static_cast<int>(a1);
            auto* addr = reinterpret_cast<net::sockaddr_in*>(a2);
            int new_fd = net::sock_accept(fd, addr);
            return (new_fd >= 0) ? new_fd : -22;
        }

        case SYS_SENDTO: {
            int fd = static_cast<int>(a1);
            const void* buf = reinterpret_cast<const void*>(a2);
            size_t len = static_cast<size_t>(a3);
            int flags = static_cast<int>(a4);
            const auto* dest = reinterpret_cast<const net::sockaddr_in*>(a5);
            int64_t sent = net::sock_sendto(fd, buf, len, flags, dest);
            return (sent >= 0) ? sent : -9;
        }

        case SYS_RECVFROM: {
            int fd = static_cast<int>(a1);
            void* buf = reinterpret_cast<void*>(a2);
            size_t len = static_cast<size_t>(a3);
            int flags = static_cast<int>(a4);
            auto* src = reinterpret_cast<net::sockaddr_in*>(a5);
            int64_t recvd = net::sock_recvfrom(fd, buf, len, flags, src);
            return (recvd >= 0) ? recvd : -9;
        }

        case SYS_SHUTDOWN: {
            int fd = static_cast<int>(a1);
            int how = static_cast<int>(a2);
            return net::sock_shutdown(fd, how);
        }

        case SYS_BIND: {
            int fd = static_cast<int>(a1);
            const auto* addr = reinterpret_cast<const net::sockaddr_in*>(a2);
            if (!addr) return -14;
            int res = net::sock_bind(fd, addr);
            return (res == 0) ? 0 : -22;
        }

        case SYS_LISTEN: {
            int fd = static_cast<int>(a1);
            int backlog = static_cast<int>(a2);
            int res = net::sock_listen(fd, backlog);
            return (res == 0) ? 0 : -22;
        }

        case SYS_GETSOCKNAME: {
            int fd = static_cast<int>(a1);
            auto* addr = reinterpret_cast<net::sockaddr_in*>(a2);
            if (!addr) return -14;
            int res = net::sock_getsockname(fd, addr);
            return (res == 0) ? 0 : -22;
        }

        case SYS_GETPEERNAME: {
            int fd = static_cast<int>(a1);
            auto* addr = reinterpret_cast<net::sockaddr_in*>(a2);
            if (!addr) return -14;
            int res = net::sock_getpeername(fd, addr);
            return (res == 0) ? 0 : -22;
        }

        case SYS_SETSOCKOPT: {
            int fd = static_cast<int>(a1);
            int level = static_cast<int>(a2);
            int optname = static_cast<int>(a3);
            const void* optval = reinterpret_cast<const void*>(a4);
            uint32_t optlen = static_cast<uint32_t>(a5);
            return net::sock_setsockopt(fd, level, optname, optval, optlen);
        }

        case SYS_GETSOCKOPT: {
            int fd = static_cast<int>(a1);
            int level = static_cast<int>(a2);
            int optname = static_cast<int>(a3);
            void* optval = reinterpret_cast<void*>(a4);
            auto* optlen = reinterpret_cast<uint32_t*>(a5);
            return net::sock_getsockopt(fd, level, optname, optval, optlen);
        }

        case SYS_SELECT: {
            return 1;
        }

        default: {
            lib::kprint_str("[SYSCALL] Unhandled Linux syscall #");
            lib::kprint_udec(num);
            lib::kprint_str("\r\n");
            return -38;
        }
    }
}

static void user_mode_test_program() {
    const char msg[] = "[RING3 TEST] Hello from Native Ring 3 User Mode via syscall!\r\n";
    asm volatile(
        "mov $1, %%rax\n\t"
        "mov $1, %%rdi\n\t"
        "mov %0, %%rsi\n\t"
        "mov %1, %%rdx\n\t"
        "syscall\n\t"
        "mov $60, %%rax\n\t"
        "mov $42, %%rdi\n\t"
        "syscall\n\t"
        :
        : "r"(msg), "r"(sizeof(msg) - 1)
        : "rax", "rdi", "rsi", "rdx", "rcx", "r11", "memory"
    );
}

int64_t run_user_test() {
    uint64_t user_stack_bottom = 0x0000000000400000ULL;
    size_t user_stack_size     = 65536;
    for (uint64_t v = user_stack_bottom; v < user_stack_bottom + user_stack_size; v += mm::PAGE_SIZE) {
        uint64_t phys = mm::vmm_virt_to_phys(v);
        mm::vmm_map_page(v, phys, mm::PTE_PRESENT | mm::PTE_WRITABLE | mm::PTE_USER);
    }
    uint64_t user_rsp = (user_stack_bottom + user_stack_size - 16) & ~0xFULL;

    uint64_t user_rip = reinterpret_cast<uint64_t>(&user_mode_test_program);
    uint64_t code_page = user_rip & ~0xFFFULL;
    uint64_t code_phys = mm::vmm_virt_to_phys(code_page);
    mm::vmm_map_page(code_page, code_phys, mm::PTE_PRESENT | mm::PTE_WRITABLE | mm::PTE_USER);

    drivers::serial_puts("[RING3] Dropping privilege to User Mode Ring 3 (sysretq)...\r\n");
    return enter_user_mode(user_rip, user_rsp);
}

}
