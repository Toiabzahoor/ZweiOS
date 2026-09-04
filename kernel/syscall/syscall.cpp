/* ==============================================================================
 * ZweiOS - Bare-Metal x86_64 Operating System
 * Component: Fast System Call Subsystem (IA32_LSTAR / syscall / sysretq) Implementation
 * ============================================================================== */

#include "syscall/syscall.hpp"
#include "arch/x86_64/io.hpp"
#include "drivers/serial.hpp"
#include "drivers/keyboard.hpp"
#include "drivers/pit.hpp"
#include "mm/pmm.hpp"
#include "mm/vmm.hpp"
#include "lib/kprintf.hpp"
#include "lib/string.hpp"

// Assembly globals for state tracking
extern "C" uint64_t syscall_exit_requested;
extern "C" uint64_t syscall_exit_code;

static uint64_t current_process_brk = 0x0000000000600000ULL;
static uint64_t current_mmap_vaddr   = 0x0000000010000000ULL;

namespace syscall {

void syscall_init() {
    // 1. Enable System Call Extensions (SCE) in IA32_EFER
    uint64_t efer = arch::rdmsr(arch::IA32_EFER);
    arch::wrmsr(arch::IA32_EFER, efer | 1ULL); // Bit 0 = SCE

    // 2. Configure IA32_STAR:
    // Bits 47..32: Kernel CS (0x08), Kernel SS (0x10 = 0x08 + 8)
    // Bits 63..48: User CS/SS base (0x10 -> User SS 0x1B = 0x18 | 3, User CS 0x23 = 0x20 | 3)
    uint64_t star = (static_cast<uint64_t>(0x0010ULL) << 48) | (static_cast<uint64_t>(0x0008ULL) << 32);
    arch::wrmsr(arch::IA32_STAR, star);

    // 3. Configure IA32_LSTAR: Target 64-bit RIP vector for syscall
    arch::wrmsr(arch::IA32_LSTAR, reinterpret_cast<uint64_t>(&syscall_entry));

    // 4. Configure IA32_FMASK: Mask RFLAGS bits during syscall entry
    // Mask Bit 9 (IF - Interrupt Flag = 0x200), Bit 8 (TF = 0x100), Bit 10 (DF = 0x400)
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

            if (fd == 1 || fd == 2) { // stdout or stderr
                for (size_t i = 0; i < count; ++i) {
                    lib::kprint_char(buf[i]);
                }
                return static_cast<int64_t>(count);
            }
            return -9; // -EBADF
        }

        case SYS_EXIT:
        case SYS_EXIT_GROUP: {
            syscall_exit_requested = 1;
            syscall_exit_code = static_cast<uint64_t>(a1);
            return 0;
        }

        case SYS_GETPID: {
            return 1; // Initial Process ID
        }

        case SYS_READ: {
            int fd = static_cast<int>(a1);
            char* buf = reinterpret_cast<char*>(a2);
            size_t count = static_cast<size_t>(a3);
            if (!buf || count == 0) return 0;

            if (fd == 0) { // stdin
                size_t read_bytes = 0;
                while (read_bytes < count) {
                    char c = 0;
                    if (drivers::sys_try_getc(&c)) {
                        buf[read_bytes++] = c;
                    } else {
                        break;
                    }
                }
                return static_cast<int64_t>(read_bytes);
            }
            return -9; // -EBADF
        }

        case SYS_ARCH_PRCTL: {
            uint64_t code = a1;
            uint64_t addr = a2;
            if (code == ARCH_SET_FS) {
                arch::wrmsr(arch::IA32_FS_BASE, addr);
                return 0;
            } else if (code == ARCH_GET_FS) {
                if (!addr) return -14; // -EFAULT
                *reinterpret_cast<uint64_t*>(addr) = arch::rdmsr(arch::IA32_FS_BASE);
                return 0;
            } else if (code == ARCH_SET_GS) {
                arch::wrmsr(arch::IA32_GS_BASE, addr);
                return 0;
            } else if (code == ARCH_GET_GS) {
                if (!addr) return -14; // -EFAULT
                *reinterpret_cast<uint64_t*>(addr) = arch::rdmsr(arch::IA32_GS_BASE);
                return 0;
            }
            return -22; // -EINVAL
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

        case SYS_MMAP: {
            uint64_t addr = a1;
            uint64_t length = a2;
            if (length == 0) return -22; // -EINVAL
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

        case SYS_MUNMAP: {
            return 0; // Success
        }

        case SYS_WRITEV: {
            int fd = static_cast<int>(a1);
            const auto* iov = reinterpret_cast<const iovec*>(a2);
            size_t iovcnt = static_cast<size_t>(a3);
            if (!iov || iovcnt == 0) return 0;

            if (fd == 1 || fd == 2) {
                size_t total_written = 0;
                for (size_t i = 0; i < iovcnt; ++i) {
                    const char* buf = reinterpret_cast<const char*>(iov[i].iov_base);
                    size_t count = iov[i].iov_len;
                    if (buf && count > 0) {
                        for (size_t k = 0; k < count; ++k) {
                            lib::kprint_char(buf[k]);
                        }
                        total_written += count;
                    }
                }
                return static_cast<int64_t>(total_written);
            }
            return -9; // -EBADF
        }

        case SYS_IOCTL: {
            uint64_t cmd = a2;
            uint64_t arg = a3;
            if (cmd == 0x5413) { // TIOCGWINSZ
                auto* ws = reinterpret_cast<uint16_t*>(arg);
                if (ws) {
                    ws[0] = 25; // rows
                    ws[1] = 80; // cols
                    ws[2] = 0;
                    ws[3] = 0;
                }
                return 0;
            }
            return 0; // Succeed default
        }

        case SYS_UNAME: {
            auto* un = reinterpret_cast<utsname*>(a1);
            if (!un) return -14; // -EFAULT
            lib::strcpy(un->sysname, "Linux");
            lib::strcpy(un->nodename, "zweios");
            lib::strcpy(un->release, "5.15.0");
            lib::strcpy(un->version, "#1 SMP PREEMPT ZweiOS");
            lib::strcpy(un->machine, "x86_64");
            lib::strcpy(un->domainname, "local");
            return 0;
        }

        case SYS_CLOCK_GETTIME: {
            auto* tp = reinterpret_cast<timespec*>(a2);
            if (!tp) return -14; // -EFAULT
            uint64_t sec = drivers::timer_get_uptime_seconds();
            tp->tv_sec = static_cast<int64_t>(sec);
            tp->tv_nsec = static_cast<int64_t>((drivers::timer_get_ticks() % 100) * 10000000LL);
            return 0;
        }

        case SYS_GETUID:
        case SYS_GETGID:
        case SYS_GETEUID:
        case SYS_GETEGID: {
            return 1000;
        }

        default: {
            lib::kprint_str("[SYSCALL] Warning: Unhandled syscall number: ");
            lib::kprint_udec(num);
            lib::kprint_str("\r\n");
            return -38; // -ENOSYS
        }
    }
}

// Minimal Standalone User Mode Machine Code (87 bytes)
// Executes:
//   sys_write(1, "[USER] Hello from Ring 3 User Mode (syscall)!\r\n", 44)
//   sys_exit(42)
static const uint8_t ring3_test_payload[] = {
    0xB8, 0x01, 0x00, 0x00, 0x00, // mov eax, 1 (sys_write)
    0xBF, 0x01, 0x00, 0x00, 0x00, // mov edi, 1 (stdout)
    0x48, 0x8D, 0x35, 0x17, 0x00, 0x00, 0x00, // lea rsi, [rip + 23] (msg)
    0xBA, 0x2C, 0x00, 0x00, 0x00, // mov edx, 44 (length)
    0x0F, 0x05,                   // syscall
    0xB8, 0x3C, 0x00, 0x00, 0x00, // mov eax, 60 (sys_exit)
    0xBF, 0x2A, 0x00, 0x00, 0x00, // mov edi, 42 (exit code 42)
    0x0F, 0x05,                   // syscall
    0xF3, 0x90, 0xEB, 0xFC,       // pause; jmp -4
    // Message payload:
    '[', 'U', 'S', 'E', 'R', ']', ' ', 'H', 'e', 'l', 'l', 'o', ' ', 'f', 'r', 'o',
    'm', ' ', 'R', 'i', 'n', 'g', ' ', '3', ' ', 'U', 's', 'e', 'r', ' ', 'M', 'o',
    'd', 'e', ' ', '(', 's', 'y', 's', 'c', 'a', 'l', 'l', ')', '!', '\r', '\n'
};

int64_t run_user_test() {
    // We map code and stack to user memory (0x500000 = 5MB, inside lower 1GB PTE_USER range)
    uint8_t* user_code_dest = reinterpret_cast<uint8_t*>(0x500000ULL);
    uint64_t user_rsp = 0x510000ULL; // 64KB user stack

    lib::memcpy(user_code_dest, ring3_test_payload, sizeof(ring3_test_payload));

    lib::kprint_str("[SYSCALL] Preparing Ring 3 user execution context...\r\n");
    lib::kprint_str("[SYSCALL] User Code  : ");
    lib::kprint_ptr(reinterpret_cast<const void*>(0x500000ULL));
    lib::kprint_str(" (Selector 0x23, RPL 3)\r\n");
    lib::kprint_str("[SYSCALL] User Stack : ");
    lib::kprint_ptr(reinterpret_cast<const void*>(user_rsp));
    lib::kprint_str(" (Selector 0x1B, RPL 3)\r\n");
    lib::kprint_str("[SYSCALL] Dropping privilege via iretq frame...\r\n");

    int64_t exit_code = enter_user_mode(0x500000ULL, user_rsp);

    lib::kprint_str("[SYSCALL] Process exited cleanly with status code: ");
    lib::kprint_udec(static_cast<uint64_t>(exit_code));
    lib::kprint_str("\r\n");

    return exit_code;
}

} // namespace syscall
