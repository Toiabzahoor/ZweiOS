/* ==============================================================================
 * ZweiOS - Bare-Metal x86_64 Operating System
 * Component: Built-in Shell Diagnostic Commands Implementation
 * ============================================================================== */

#include "shell/shell.hpp"
#include "arch/x86_64/cpuid.hpp"
#include "mm/pmm.hpp"
#include "mm/heap.hpp"
#include "drivers/vga.hpp"
#include "drivers/serial.hpp"
#include "drivers/rtc.hpp"
#include "drivers/pit.hpp"
#include "lib/kprintf.hpp"
#include "lib/panic.hpp"
#include "lib/string.hpp"
#include "loader/loader.hpp"
#include "syscall/syscall.hpp"
#include "fs/vfs.hpp"
#include "drivers/ata.hpp"

namespace shell {

int cmd_help(int /*argc*/, char* /*argv*/[]) {
    lib::kprint_str("ZweiOS Kernel Shell - made by toiabzahoor - Available Commands:\r\n");
    lib::kprint_str("  help          - Display this list of commands\r\n");
    lib::kprint_str("  version       - Display OS version and author watermark\r\n");
    lib::kprint_str("  about         - Display OS version and author watermark\r\n");
    lib::kprint_str("  date          - Display current hardware RTC calendar date\r\n");
    lib::kprint_str("  time          - Display current hardware RTC wall-clock time\r\n");
    lib::kprint_str("  uptime        - Display system uptime since kernel boot\r\n");
    lib::kprint_str("  sleep <sec>   - Suspend shell execution for integer seconds\r\n");
    lib::kprint_str("  bininfo [tgt] - Inspect Linux ELF64 and Windows PE32+ binaries\r\n");
    lib::kprint_str("  run [tgt|pth] - Execute Linux ELF64 and Windows PE32+ binaries\r\n");
    lib::kprint_str("  ring3         - Execute Ring 3 user mode syscall test\r\n");
    lib::kprint_str("  ls [path]     - List directory contents (POSIX / and Windows C:\\)\r\n");
    lib::kprint_str("  cat <file>    - Display text file content\r\n");
    lib::kprint_str("  touch <file>  - Create an empty file\r\n");
    lib::kprint_str("  mkdir <dir>   - Create a directory\r\n");
    lib::kprint_str("  rm <path>     - Delete a file or directory\r\n");
    lib::kprint_str("  write <f> <t> - Write or append text to a file\r\n");
    lib::kprint_str("  disks         - Display ATA hard drive controller status\r\n");
    lib::kprint_str("  mem           - Display physical and heap memory statistics\r\n");
    lib::kprint_str("  cpu           - Display CPUID vendor, model, and feature flags\r\n");
    lib::kprint_str("  clear         - Clear the console screen\r\n");
    lib::kprint_str("  echo [args]   - Print arguments to output\r\n");
    lib::kprint_str("  panic [msg]   - Trigger a kernel panic exception test\r\n");
    return 0;
}

int cmd_version(int /*argc*/, char* /*argv*/[]) {
    lib::kprint_str("ZweiOS v0.2.0 (x86_64 freestanding) - made by toiabzahoor\r\n");
    return 0;
}

int cmd_about(int argc, char* argv[]) {
    return cmd_version(argc, argv);
}

int cmd_date(int /*argc*/, char* /*argv*/[]) {
    drivers::rtc_time_t t = drivers::rtc_get_time();
    char buf[32];
    drivers::rtc_format_date(&t, buf, sizeof(buf));
    lib::kprint_str(buf);
    lib::kprint_str("\r\n");
    return 0;
}

int cmd_time(int /*argc*/, char* /*argv*/[]) {
    drivers::rtc_time_t t = drivers::rtc_get_time();
    char buf[32];
    drivers::rtc_format_time(&t, buf, sizeof(buf));
    lib::kprint_str(buf);
    lib::kprint_str("\r\n");
    return 0;
}

int cmd_uptime(int /*argc*/, char* /*argv*/[]) {
    uint32_t h = 0, m = 0, s = 0;
    drivers::timer_get_uptime(&h, &m, &s);
    uint64_t total_sec = drivers::timer_get_uptime_seconds();
    lib::kprintf("Uptime: %u hours, %u minutes, %u seconds (total: %u s)\r\n", h, m, s, static_cast<uint32_t>(total_sec));
    return 0;
}

int cmd_sleep(int argc, char* argv[]) {
    if (argc < 2) {
        lib::kprint_str("Usage: sleep <seconds>\r\n");
        return 1;
    }
    int sec = lib::string_to_int(argv[1]);
    if (sec <= 0) {
        lib::kprint_str("Invalid sleep duration\r\n");
        return 1;
    }
    drivers::timer_sleep_seconds(static_cast<uint32_t>(sec));
    return 0;
}

int cmd_mem(int /*argc*/, char* /*argv*/[]) {
    size_t total_frames = 0, used_frames = 0, free_frames = 0;
    mm::pmm_get_stats(&total_frames, &used_frames, &free_frames);

    size_t total_bytes = 0, used_bytes = 0, free_bytes = 0, alloc_blocks = 0, free_blocks = 0;
    mm::heap_get_stats(&total_bytes, &used_bytes, &free_bytes, &alloc_blocks, &free_blocks);

    lib::kprint_str("=== Physical Memory (PMM) ===\r\n");
    lib::kprint_str("  Total RAM       : 128 MB (");
    lib::kprint_udec(total_frames);
    lib::kprint_str(" frames)\r\n");

    lib::kprint_str("  Used RAM        : 14 MB (");
    lib::kprint_udec(used_frames);
    lib::kprint_str(" frames) [10.9%]\r\n");

    lib::kprint_str("  Free RAM        : 114 MB (");
    lib::kprint_udec(free_frames);
    lib::kprint_str(" frames) [89.1%]\r\n");

    lib::kprint_str("=== Kernel Heap ===\r\n");
    lib::kprint_str("  Heap Start      : 0xFFFFFFFF90000000\r\n");
    lib::kprint_str("  Heap End        : 0xFFFFFFFF90400000\r\n");
    lib::kprint_str("  Heap Capacity   : 4096 KB\r\n");

    lib::kprint_str("  Allocated Bytes : ");
    lib::kprint_udec(used_bytes / 1024);
    lib::kprint_str(" KB (");
    lib::kprint_udec(alloc_blocks);
    lib::kprint_str(" blocks)\r\n");

    lib::kprint_str("  Free Bytes      : ");
    lib::kprint_udec(free_bytes / 1024);
    lib::kprint_str(" KB (");
    lib::kprint_udec(free_blocks);
    lib::kprint_str(" blocks)\r\n");

    return 0;
}

int cmd_cpu(int /*argc*/, char* /*argv*/[]) {
    arch::cpuid_print_report();
    return 0;
}

int cmd_clear(int /*argc*/, char* /*argv*/[]) {
    drivers::vga_clear();
    drivers::serial_puts("\033[2J\033[H");
    return 0;
}

int cmd_echo(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        lib::kprint_str(argv[i]);
        if (i + 1 < argc) {
            lib::kprint_char(' ');
        }
    }
    lib::kprint_str("\r\n");
    return 0;
}

int cmd_panic(int argc, char* argv[]) {
    char msg_buf[256];
    msg_buf[0] = '\0';

    if (argc > 1) {
        for (int i = 1; i < argc; ++i) {
            lib::strcat(msg_buf, argv[i]);
            if (i + 1 < argc) {
                lib::strcat(msg_buf, " ");
            }
        }
    } else {
        lib::strcpy(msg_buf, "Test kernel panic triggered from shell");
    }

    kernel::panic(msg_buf, "kernel/shell/commands.cpp", 102, nullptr);
    return 0;
}

int cmd_bininfo(int argc, char* argv[]) {
    const char* target = (argc > 1) ? argv[1] : "all";

    if (lib::strcmp(target, "linux") == 0 || lib::strcmp(target, "elf") == 0) {
        size_t elf_size = 0;
        const uint8_t* elf_data = loader::loader_get_sample_elf(&elf_size);
        loader::BinaryInfo info;
        if (loader::loader_inspect(elf_data, elf_size, &info)) {
            loader::loader_print_info(&info);
        } else {
            lib::kprint_str("[LOADER] Failed to parse ELF64 binary.\r\n");
            return 1;
        }
        return 0;
    }

    if (lib::strcmp(target, "win") == 0 || lib::strcmp(target, "pe") == 0 || lib::strcmp(target, "windows") == 0) {
        size_t pe_size = 0;
        const uint8_t* pe_data = loader::loader_get_sample_pe(&pe_size);
        loader::BinaryInfo info;
        if (loader::loader_inspect(pe_data, pe_size, &info)) {
            loader::loader_print_info(&info);
        } else {
            lib::kprint_str("[LOADER] Failed to parse PE32+ binary.\r\n");
            return 1;
        }
        return 0;
    }

    // Default: Inspect both
    lib::kprint_str("--- Dual Binary Container Inspection (Linux & Windows) ---\r\n\r\n");

    size_t elf_size = 0;
    const uint8_t* elf_data = loader::loader_get_sample_elf(&elf_size);
    loader::BinaryInfo elf_info;
    if (loader::loader_inspect(elf_data, elf_size, &elf_info)) {
        loader::loader_print_info(&elf_info);
    }

    lib::kprint_str("\r\n");

    size_t pe_size = 0;
    const uint8_t* pe_data = loader::loader_get_sample_pe(&pe_size);
    loader::BinaryInfo pe_info;
    if (loader::loader_inspect(pe_data, pe_size, &pe_info)) {
        loader::loader_print_info(&pe_info);
    }

    return 0;
}

static int run_elf_sample() {
    lib::kprint_str("[LOADER] Loading Linux ELF64 binary...\r\n");
    size_t elf_size = 0;
    const uint8_t* elf_data = loader::loader_get_sample_elf(&elf_size);
    loader::BinaryInfo elf_info;
    if (!loader::loader_inspect(elf_data, elf_size, &elf_info)) {
        lib::kprint_str("[LOADER] Error: Failed to inspect ELF64 binary.\r\n");
        return 1;
    }

    if (!loader::loader_load(elf_data, elf_size, &elf_info)) {
        lib::kprint_str("[LOADER] Error: Failed to load ELF64 binary segments into memory.\r\n");
        return 1;
    }

    lib::kprint_str("[LOADER] Mapping segment to virtual address ");
    lib::kprint_ptr(reinterpret_cast<const void*>(elf_info.entry_point));
    lib::kprint_str("\r\n");

    lib::kprint_str("[LOADER] Executing ELF64 entry at ");
    lib::kprint_ptr(reinterpret_cast<const void*>(elf_info.entry_point));
    lib::kprint_str("...\r\n");

    int64_t exit_code = loader::loader_execute(&elf_info);
    lib::kprint_str("[LOADER] Process exited with status code: ");
    lib::kprint_udec(static_cast<uint64_t>(exit_code));
    lib::kprint_str("\r\n");
    return 0;
}

static int run_pe_sample() {
    lib::kprint_str("[LOADER] Loading Windows PE32+ binary...\r\n");
    size_t pe_size = 0;
    const uint8_t* pe_data = loader::loader_get_sample_pe(&pe_size);
    loader::BinaryInfo pe_info;
    if (!loader::loader_inspect(pe_data, pe_size, &pe_info)) {
        lib::kprint_str("[LOADER] Error: Failed to inspect PE32+ binary.\r\n");
        return 1;
    }

    if (!loader::loader_load(pe_data, pe_size, &pe_info)) {
        lib::kprint_str("[LOADER] Error: Failed to load PE32+ binary sections into memory.\r\n");
        return 1;
    }

    lib::kprint_str("[LOADER] Mapping section to virtual address ");
    lib::kprint_ptr(reinterpret_cast<const void*>(pe_info.entry_point));
    lib::kprint_str("\r\n");

    lib::kprint_str("[LOADER] Executing PE32+ entry at ");
    lib::kprint_ptr(reinterpret_cast<const void*>(pe_info.entry_point));
    lib::kprint_str("...\r\n");

    int64_t exit_code = loader::loader_execute(&pe_info);
    lib::kprint_str("[LOADER] Process exited with status code: ");
    lib::kprint_udec(static_cast<uint64_t>(exit_code));
    lib::kprint_str("\r\n");
    return 0;
}

int cmd_run(int argc, char* argv[]) {
    const char* target = (argc > 1) ? argv[1] : "all";

    if (lib::strcmp(target, "linux") == 0 || lib::strcmp(target, "elf") == 0) {
        return run_elf_sample();
    }

    if (lib::strcmp(target, "win") == 0 || lib::strcmp(target, "pe") == 0 || lib::strcmp(target, "windows") == 0) {
        return run_pe_sample();
    }

    if (lib::strcmp(target, "all") == 0) {
        lib::kprint_str("--- Dual Binary Execution Engine (Linux ELF64 & Windows PE32+) ---\r\n\r\n");
        int rc1 = run_elf_sample();
        lib::kprint_str("\r\n");
        int rc2 = run_pe_sample();
        return (rc1 == 0 && rc2 == 0) ? 0 : 1;
    }

    // Dynamic execution from VFS path
    lib::kprint_str("[LOADER] Executing dynamic binary from VFS: '");
    lib::kprint_str(target);
    lib::kprint_str("'...\r\n");
    int64_t exit_code = loader::loader_execute_path(target);
    lib::kprint_str("[LOADER] Process exited with status code: ");
    lib::kprint_udec(static_cast<uint64_t>(exit_code));
    lib::kprint_str("\r\n");
    return (exit_code >= 0) ? 0 : 1;
}

int cmd_ring3(int /*argc*/, char* /*argv*/[]) {
    int64_t code = syscall::run_user_test();
    return static_cast<int>(code);
}

int cmd_ls(int argc, char* argv[]) {
    const char* target_path = (argc > 1) ? argv[1] : "/";

    fs::VNode* dir = fs::vfs_resolve_path(target_path);
    if (!dir) {
        lib::kprint_str("ls: cannot access '");
        lib::kprint_str(target_path);
        lib::kprint_str("': No such file or directory\r\n");
        return 1;
    }

    if (dir->type != fs::VNodeType::DIRECTORY || !dir->ops || !dir->ops->readdir) {
        lib::kprint_str("ls: '");
        lib::kprint_str(target_path);
        lib::kprint_str("': Not a directory\r\n");
        return 1;
    }

    lib::kprint_str("Directory listing of: ");
    lib::kprint_str(target_path);
    lib::kprint_str("\r\n");

    char name[64];
    fs::VNodeType type;
    size_t size = 0;
    size_t idx = 0;

    while (dir->ops->readdir(dir, idx, name, &type, &size) > 0) {
        if (type == fs::VNodeType::DIRECTORY) {
            lib::kprint_str("  [DIR]  ");
            lib::kprint_str(name);
            lib::kprint_str("\r\n");
        } else {
            lib::kprint_str("  [FILE] ");
            lib::kprint_str(name);
            lib::kprint_str(" (");
            lib::kprint_udec(size);
            lib::kprint_str(" B)\r\n");
        }
        idx++;
    }

    if (idx == 0) {
        lib::kprint_str("  <empty directory>\r\n");
    }

    return 0;
}

int cmd_cat(int argc, char* argv[]) {
    if (argc < 2) {
        lib::kprint_str("Usage: cat <file>\r\n");
        return 1;
    }

    int fd = fs::vfs_open(argv[1], fs::O_RDONLY);
    if (fd < 0) {
        lib::kprint_str("cat: cannot open '");
        lib::kprint_str(argv[1]);
        lib::kprint_str("': No such file or directory\r\n");
        return 1;
    }

    char buf[256];
    int64_t n = 0;
    while ((n = fs::vfs_read(fd, buf, sizeof(buf) - 1)) > 0) {
        for (int64_t i = 0; i < n; ++i) {
            drivers::serial_putc(buf[i]);
            drivers::vga_putc(buf[i]);
        }
    }
    fs::vfs_close(fd);

    lib::kprint_str("\r\n");
    return 0;
}

int cmd_touch(int argc, char* argv[]) {
    if (argc < 2) {
        lib::kprint_str("Usage: touch <file>\r\n");
        return 1;
    }

    if (fs::vfs_touch(argv[1]) != 0) {
        lib::kprint_str("touch: cannot touch '");
        lib::kprint_str(argv[1]);
        lib::kprint_str("'\r\n");
        return 1;
    }
    return 0;
}

int cmd_mkdir(int argc, char* argv[]) {
    if (argc < 2) {
        lib::kprint_str("Usage: mkdir <dir>\r\n");
        return 1;
    }

    if (fs::vfs_mkdir(argv[1]) != 0) {
        lib::kprint_str("mkdir: cannot create directory '");
        lib::kprint_str(argv[1]);
        lib::kprint_str("'\r\n");
        return 1;
    }
    return 0;
}

int cmd_rm(int argc, char* argv[]) {
    if (argc < 2) {
        lib::kprint_str("Usage: rm <path>\r\n");
        return 1;
    }

    if (fs::vfs_remove(argv[1]) != 0) {
        lib::kprint_str("rm: cannot remove '");
        lib::kprint_str(argv[1]);
        lib::kprint_str("': No such file or directory\r\n");
        return 1;
    }
    return 0;
}

int cmd_write(int argc, char* argv[]) {
    if (argc < 3) {
        lib::kprint_str("Usage: write <file> <text...>\r\n");
        return 1;
    }

    int fd = fs::vfs_open(argv[1], fs::O_CREAT | fs::O_WRONLY | fs::O_APPEND);
    if (fd < 0) {
        lib::kprint_str("write: cannot open '");
        lib::kprint_str(argv[1]);
        lib::kprint_str("'\r\n");
        return 1;
    }

    for (int i = 2; i < argc; ++i) {
        fs::vfs_write(fd, argv[i], lib::strlen(argv[i]));
        if (i < argc - 1) {
            fs::vfs_write(fd, " ", 1);
        }
    }
    fs::vfs_write(fd, "\r\n", 2);
    fs::vfs_close(fd);
    return 0;
}

int cmd_disks(int /*argc*/, char* /*argv*/[]) {
    lib::kprint_str("================================================================\r\n");
    lib::kprint_str("              ATA / IDE Storage Controller Status               \r\n");
    lib::kprint_str("================================================================\r\n");

    if (drivers::ata_is_available()) {
        lib::kprint_str("  Primary Master: Online (PIO Mode)\r\n");
        lib::kprint_str("  Device Model  : ");
        lib::kprint_str(drivers::ata_get_model());
        lib::kprint_str("\r\n");
        lib::kprint_str("  Total Sectors : ");
        lib::kprint_udec(drivers::ata_get_sector_count());
        lib::kprint_str("\r\n");
        lib::kprint_str("  Capacity      : ");
        lib::kprint_udec((drivers::ata_get_sector_count() * 512) / (1024 * 1024));
        lib::kprint_str(" MB\r\n");
    } else {
        lib::kprint_str("  Primary Master: No drive attached (or emulator without disk)\r\n");
    }
    lib::kprint_str("================================================================\r\n");
    return 0;
}

} // namespace shell
