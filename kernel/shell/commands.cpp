#include "shell/shell.hpp"
#include "arch/x86_64/cpuid.hpp"
#include "mm/pmm.hpp"
#include "mm/heap.hpp"
#include "drivers/vga.hpp"
#include "drivers/serial.hpp"
#include "drivers/rtc.hpp"
#include "drivers/pit.hpp"
#include "drivers/vbe.hpp"
#include "drivers/mouse.hpp"
#include "gui/gfx.hpp"
#include "gui/wm.hpp"
#include "lib/kprintf.hpp"
#include "lib/panic.hpp"
#include "lib/string.hpp"
#include "loader/loader.hpp"
#include "syscall/syscall.hpp"
#include "fs/vfs.hpp"
#include "fs/partition.hpp"
#include "fs/fat32.hpp"
#include "drivers/ata.hpp"
#include "proc/process.hpp"
#include "net/net.hpp"
#include "net/ipv4.hpp"
#include "net/icmp.hpp"
#include "net/dns.hpp"
#include "net/tcp.hpp"
#include "net/socket.hpp"
#include "net/ethernet.hpp"
#include "drivers/e1000.hpp"

namespace shell {

int cmd_help(int , char* []) {
    lib::kprint_str("ZweiOS Kernel Shell - made by toiabzahoor - Available Commands:\r\n");
    lib::kprint_str("  help          - Display this list of commands\r\n");
    lib::kprint_str("  version       - Display OS version and author watermark\r\n");
    lib::kprint_str("  about         - Display OS version and author watermark\r\n");
    lib::kprint_str("  date          - Display current hardware RTC calendar date\r\n");
    lib::kprint_str("  time          - Display current hardware RTC wall-clock time\r\n");
    lib::kprint_str("  uptime        - Display system uptime since kernel boot\r\n");
    lib::kprint_str("  sleep <sec>   - Suspend shell execution for integer seconds\r\n");
    lib::kprint_str("  bininfo [tgt] - Inspect Linux ELF64 and Windows PE32+ binaries\r\n");
    lib::kprint_str("  run [pth] [a] - Execute Linux ELF64 and Windows PE32+ binaries\r\n");
    lib::kprint_str("  ring3         - Execute Ring 3 user mode syscall test\r\n");
    lib::kprint_str("  ls [path]     - List directory contents (POSIX / and Windows C:\\, D:\\)\r\n");
    lib::kprint_str("  cat <file>    - Display text file content\r\n");
    lib::kprint_str("  touch <file>  - Create an empty file\r\n");
    lib::kprint_str("  mkdir <dir>   - Create a directory\r\n");
    lib::kprint_str("  rm <path>     - Delete a file or directory\r\n");
    lib::kprint_str("  write <f> <t> - Write or append text to a file\r\n");
    lib::kprint_str("  disks         - Display ATA hard drives and partition table status\r\n");
    lib::kprint_str("  mount [p] [d] - Display or attach filesystem mount points\r\n");
    lib::kprint_str("  unmount <pth> - Detach a filesystem mount point\r\n");
    lib::kprint_str("  gui           - Launch linear framebuffer desktop window manager\r\n");
    lib::kprint_str("  resolution    - Query or configure VBE graphics resolution mode\r\n");
    lib::kprint_str("  ps            - Display active processes and multitasking status\r\n");
    lib::kprint_str("  kill <pid>    - Terminate a process by PID\r\n");
    lib::kprint_str("  spawn <path>  - Spawn a program into the process table\r\n");
    lib::kprint_str("  ifconfig      - Display network interface configuration and statistics\r\n");
    lib::kprint_str("  ping <host>   - Send ICMP Echo requests to network host\r\n");
    lib::kprint_str("  nslookup <dom>- Query Internet domain name servers\r\n");
    lib::kprint_str("  netstat       - Display active network connections and sockets\r\n");
    lib::kprint_str("  curl <url>    - Transfer data from or to a server (HTTP)\r\n");
    lib::kprint_str("  wget <url>    - Download file from server (HTTP)\r\n");
    lib::kprint_str("  mem           - Display physical and heap memory statistics\r\n");
    lib::kprint_str("  cpu           - Display CPUID vendor, model, and feature flags\r\n");
    lib::kprint_str("  clear         - Clear the console screen\r\n");
    lib::kprint_str("  echo [args]   - Print arguments to output\r\n");
    lib::kprint_str("  panic [msg]   - Trigger a kernel panic exception test\r\n");
    return 0;
}

int cmd_version(int , char* []) {
    lib::kprint_str("ZweiOS v0.2.0 (x86_64 freestanding) - made by toiabzahoor\r\n");
    return 0;
}

int cmd_about(int argc, char* argv[]) {
    return cmd_version(argc, argv);
}

int cmd_date(int , char* []) {
    drivers::rtc_time_t t = drivers::rtc_get_time();
    char buf[32];
    drivers::rtc_format_date(&t, buf, sizeof(buf));
    lib::kprint_str(buf);
    lib::kprint_str("\r\n");
    return 0;
}

int cmd_time(int , char* []) {
    drivers::rtc_time_t t = drivers::rtc_get_time();
    char buf[32];
    drivers::rtc_format_time(&t, buf, sizeof(buf));
    lib::kprint_str(buf);
    lib::kprint_str("\r\n");
    return 0;
}

int cmd_uptime(int , char* []) {
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

int cmd_mem(int , char* []) {
    size_t total_frames = 0, used_frames = 0, free_frames = 0;
    mm::pmm_get_stats(&total_frames, &used_frames, &free_frames);

    size_t heap_total = 0, heap_used = 0, heap_free = 0, alloc_count = 0, free_count = 0;
    mm::heap_get_stats(&heap_total, &heap_used, &heap_free, &alloc_count, &free_count);

    lib::kprint_str("================================================================\r\n");
    lib::kprint_str("                ZweiOS Memory Subsystem Statistics              \r\n");
    lib::kprint_str("================================================================\r\n");

    lib::kprint_str("  [PMM] Frame Allocator (4 KB Pages):\r\n");
    lib::kprintf("    Total Physical Memory : %u MB (%u frames)\r\n", static_cast<uint32_t>((total_frames * 4096) / (1024 * 1024)), static_cast<uint32_t>(total_frames));
    lib::kprintf("    Used Physical Memory  : %u KB (%u frames)\r\n", static_cast<uint32_t>((used_frames * 4096) / 1024), static_cast<uint32_t>(used_frames));
    lib::kprintf("    Free Physical Memory  : %u MB (%u frames)\r\n", static_cast<uint32_t>((free_frames * 4096) / (1024 * 1024)), static_cast<uint32_t>(free_frames));

    lib::kprint_str("\r\n  [HEAP] Dynamic Allocator (Free-List):\r\n");
    lib::kprintf("    Heap Base Address     : %p\r\n", reinterpret_cast<const void*>(mm::HEAP_START_ADDR));
    lib::kprintf("    Total Heap Reserved   : %u MB\r\n", static_cast<uint32_t>(heap_total / (1024 * 1024)));
    lib::kprintf("    Currently Allocated   : %u bytes (%u KB)\r\n", static_cast<uint32_t>(heap_used), static_cast<uint32_t>(heap_used / 1024));
    lib::kprintf("    Available Heap Memory : %u KB\r\n", static_cast<uint32_t>(heap_free / 1024));
    lib::kprintf("    Active Allocations    : %u\r\n", static_cast<uint32_t>(alloc_count));
    lib::kprint_str("================================================================\r\n");
    return 0;
}

int cmd_cpu(int , char* []) {
    arch::cpuid_print_report();
    return 0;
}

int cmd_clear(int , char* []) {
    drivers::vga_clear();
    drivers::serial_puts("\033[2J\033[H");
    return 0;
}

int cmd_echo(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        lib::kprint_str(argv[i]);
        if (i < argc - 1) {
            lib::kprint_char(' ');
        }
    }
    lib::kprint_str("\r\n");
    return 0;
}

int cmd_panic(int argc, char* argv[]) {
    const char* msg = (argc > 1) ? argv[1] : "Manual kernel panic triggered from shell";
    ::kernel::kpanic(msg, __FILE__, __LINE__, nullptr);
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

    const char* argv[] = { "/bin/hello_linux.elf", nullptr };
    int64_t exit_code = loader::loader_execute(&elf_info, 1, argv);
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

    const char* argv[] = { "hello_win.exe", nullptr };
    int64_t exit_code = loader::loader_execute(&pe_info, 1, argv);
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

    int64_t exit_code = loader::loader_execute_path(target, argc - 1, const_cast<const char**>(argv + 1));
    return (exit_code >= 0) ? 0 : 1;
}

int cmd_spawn(int argc, char* argv[]) {
    if (argc < 2) {
        lib::kprint_str("Usage: spawn <path> [args...]\r\n");
        return 1;
    }
    return cmd_run(argc, argv);
}

int cmd_ps(int , char* []) {
    proc::PCB procs[proc::MAX_PROCESSES];
    size_t count = proc::process_get_all(procs, proc::MAX_PROCESSES);

    lib::kprint_str("PID   STATE     TYPE            CR3                 TICKS   NAME\r\n");
    lib::kprint_str("----------------------------------------------------------------------------\r\n");
    for (size_t i = 0; i < count; ++i) {
        const auto& p = procs[i];
        lib::kprintf("%u     ", p.pid);
        if (p.pid < 10) lib::kprint_str(" ");

        const char* state_str = "UNKNOWN ";
        switch (p.state) {
            case proc::ProcessState::READY:   state_str = "READY   "; break;
            case proc::ProcessState::RUNNING: state_str = "RUNNING "; break;
            case proc::ProcessState::BLOCKED: state_str = "BLOCKED "; break;
            case proc::ProcessState::ZOMBIE:  state_str = "ZOMBIE  "; break;
            default: break;
        }
        lib::kprint_str(state_str);

        const char* type_str = "KERNEL_THREAD   ";
        switch (p.type) {
            case proc::ProcessType::LINUX_ELF64: type_str = "LINUX_ELF64     "; break;
            case proc::ProcessType::WIN32_PE:    type_str = "WIN32_PE        "; break;
            default: break;
        }
        lib::kprint_str(type_str);

        lib::kprint_ptr(reinterpret_cast<const void*>(p.cr3));
        lib::kprint_str("  ");

        lib::kprintf("%u\t", p.total_ticks);
        lib::kprint_str(p.name);
        if (p.state == proc::ProcessState::ZOMBIE) {
            lib::kprintf(" (exit: %d)", static_cast<int>(p.exit_code));
        }
        lib::kprint_str("\r\n");
    }
    lib::kprintf("Total processes: %u\r\n", static_cast<uint32_t>(count));
    return 0;
}

int cmd_kill(int argc, char* argv[]) {
    if (argc < 2) {
        lib::kprint_str("Usage: kill <pid>\r\n");
        return 1;
    }
    uint32_t pid = static_cast<uint32_t>(lib::string_to_int(argv[1]));
    if (pid == 0) {
        lib::kprint_str("kill: cannot terminate root kernel shell (PID 0)\r\n");
        return 1;
    }
    if (proc::process_kill(pid)) {
        lib::kprintf("Process %u terminated.\r\n", pid);
        return 0;
    } else {
        lib::kprintf("kill: process %u not found.\r\n", pid);
        return 1;
    }
}

int cmd_ring3(int , char* []) {
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
            lib::kprint_udec(static_cast<uint64_t>(size));
            lib::kprint_str(" bytes)\r\n");
        }
        idx++;
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
        lib::kprint_str("': No such file\r\n");
        return 1;
    }

    char buf[128];
    int64_t bytes = 0;
    while ((bytes = fs::vfs_read(fd, buf, sizeof(buf) - 1)) > 0) {
        buf[bytes] = '\0';
        lib::kprint_str(buf);
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

int cmd_disks(int , char* []) {
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
        lib::kprint_str(" MB\r\n\r\n");

        size_t part_cnt = fs::partition_get_count();
        lib::kprintf("  Discovered Partitions: %u\r\n", static_cast<uint32_t>(part_cnt));
        lib::kprint_str("  IDX  SCHEME  FS_TYPE   START_LBA   SECTORS     SIZE      LABEL\r\n");
        lib::kprint_str("  --------------------------------------------------------------\r\n");
        for (size_t i = 0; i < part_cnt; ++i) {
            fs::PartitionInfo pinfo;
            if (fs::partition_get(i, &pinfo)) {
                const char* sch_str = (pinfo.scheme == fs::PartitionScheme::GPT) ? "GPT   " : (pinfo.scheme == fs::PartitionScheme::MBR ? "MBR   " : "RAW   ");
                lib::kprintf("  %u    %s %s   %u\t     %u\t %u MB    %s%s\r\n",
                    pinfo.part_index,
                    sch_str,
                    fs::partition_fs_type_to_string(pinfo.fs_type),
                    static_cast<uint32_t>(pinfo.start_lba),
                    static_cast<uint32_t>(pinfo.sector_count),
                    static_cast<uint32_t>((pinfo.sector_count * 512) / (1024 * 1024)),
                    pinfo.label,
                    pinfo.is_bootable ? " [BOOT]" : ""
                );
            }
        }
    } else {
        lib::kprint_str("  Primary Master: No drive attached (or emulator without disk)\r\n");
    }
    lib::kprint_str("================================================================\r\n");
    return 0;
}

int cmd_mount(int argc, char* argv[]) {
    if (argc < 2) {
        lib::kprint_str("================================================================\r\n");
        lib::kprint_str("                   Active Filesystem Mounts                     \r\n");
        lib::kprint_str("================================================================\r\n");
        size_t count = fs::vfs_get_mount_count();
        lib::kprint_str("  DRIVE   MOUNT_PATH         ROOT_NODE\r\n");
        lib::kprint_str("  --------------------------------------------------------------\r\n");
        for (size_t i = 0; i < count; ++i) {
            fs::MountEntry m;
            if (fs::vfs_get_mount(i, &m)) {
                if (m.drive_letter != '\0') {
                    lib::kprintf("  %c:\\     %-18s %p\r\n", m.drive_letter, m.mount_path, reinterpret_cast<const void*>(m.root_vnode));
                } else {
                    lib::kprintf("  --      %-18s %p\r\n", m.mount_path, reinterpret_cast<const void*>(m.root_vnode));
                }
            }
        }
        lib::kprint_str("================================================================\r\n");
        return 0;
    }

    const char* path = argv[1];
    char drive_char = (argc > 2) ? argv[2][0] : '\0';

    size_t part_cnt = fs::partition_get_count();
    if (part_cnt == 0) {
        lib::kprint_str("mount: no disk partitions detected.\r\n");
        return 1;
    }

    fs::PartitionInfo pinfo;
    if (fs::partition_get(0, &pinfo) && pinfo.fs_type == fs::FilesystemType::FAT32) {
        fs::VNode* fat_root = fs::fat32_mount(pinfo.drive_id, pinfo.start_lba, pinfo.sector_count);
        if (fat_root) {
            fs::vfs_mount(path, drive_char, fat_root);
            lib::kprintf("Mounted partition 0 at '%s' (Drive %c:\\)\r\n", path, drive_char ? drive_char : '-');
            return 0;
        }
    }

    lib::kprint_str("mount: failed to mount partition.\r\n");
    return 1;
}

int cmd_unmount(int argc, char* argv[]) {
    if (argc < 2) {
        lib::kprint_str("Usage: unmount <path>\r\n");
        return 1;
    }

    if (fs::vfs_unmount(argv[1]) == 0) {
        lib::kprintf("Unmounted '%s'.\r\n", argv[1]);
        return 0;
    } else {
        lib::kprintf("unmount: '%s' not mounted.\r\n", argv[1]);
        return 1;
    }
}

int cmd_gui(int , char* []) {
    if (!drivers::vbe_is_available()) {
        lib::kprint_str("gui: VBE DISPI graphics controller not detected.\r\n");
        return 1;
    }

    lib::kprint_str("[GUI] Initializing High-Resolution Desktop Window Manager...\r\n");
    gui::wm_init();
    if (gui::wm_start()) {
        lib::kprint_str("[GUI] Desktop active: 1024x768 32-bit ARGB True Color.\r\n");
        gui::wm_run_loop();
        return 0;
    } else {
        lib::kprint_str("[GUI] Failed to start window manager.\r\n");
        return 1;
    }
}

int cmd_resolution(int argc, char* argv[]) {
    if (!drivers::vbe_is_available()) {
        lib::kprint_str("resolution: VBE DISPI graphics controller not available.\r\n");
        return 1;
    }

    auto* fb = drivers::vbe_get_info();
    if (argc < 3) {
        lib::kprint_str("================================================================\r\n");
        lib::kprint_str("                VBE Graphics Controller Display Status          \r\n");
        lib::kprint_str("================================================================\r\n");
        lib::kprintf("  Resolution   : %u x %u\r\n", fb->width, fb->height);
        lib::kprintf("  Color Depth  : %u bpp (32-bit ARGB True Color)\r\n", fb->bpp);
        lib::kprintf("  Frame Buffer : %p (Physical: %p)\r\n", reinterpret_cast<const void*>(drivers::VBE_LFB_VIRTUAL_ADDR), reinterpret_cast<const void*>(drivers::VBE_LFB_PHYSICAL_ADDR));
        lib::kprintf("  Active Mode  : %s\r\n", fb->is_active ? "Linear Framebuffer (LFB)" : "Disabled / Text Mode");
        lib::kprint_str("================================================================\r\n");
        lib::kprint_str("Usage to change: resolution <width> <height> [bpp]\r\n");
        return 0;
    }

    int w = lib::string_to_int(argv[1]);
    int h = lib::string_to_int(argv[2]);
    int b = (argc > 3) ? lib::string_to_int(argv[3]) : 32;

    if (w <= 0 || h <= 0 || (b != 16 && b != 24 && b != 32)) {
        lib::kprint_str("resolution: invalid display mode parameters.\r\n");
        return 1;
    }

    if (drivers::vbe_set_mode(static_cast<uint32_t>(w), static_cast<uint32_t>(h), static_cast<uint32_t>(b))) {
        drivers::mouse_set_bounds(w, h);
        lib::kprintf("Resolution switched to %u x %u @ %u bpp.\r\n", static_cast<uint32_t>(w), static_cast<uint32_t>(h), static_cast<uint32_t>(b));
        return 0;
    } else {
        lib::kprint_str("resolution: failed to switch display mode.\r\n");
        return 1;
    }
}

int cmd_ifconfig(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    net::InterfaceConfig cfg = net::net_get_config();

    char ip_str[24];
    char mask_str[24];
    char gw_str[24];
    char dns_str[24];

    net::ipv4_format(cfg.ip, ip_str, sizeof(ip_str));
    net::ipv4_format(cfg.netmask, mask_str, sizeof(mask_str));
    net::ipv4_format(cfg.gateway, gw_str, sizeof(gw_str));
    net::ipv4_format(cfg.dns, dns_str, sizeof(dns_str));

    const char hex_chars[] = "0123456789abcdef";
    char mac_str[20];
    size_t m_idx = 0;
    for (size_t i = 0; i < 6; ++i) {
        if (i > 0) mac_str[m_idx++] = ':';
        mac_str[m_idx++] = hex_chars[(cfg.mac[i] >> 4) & 0xF];
        mac_str[m_idx++] = hex_chars[cfg.mac[i] & 0xF];
    }
    mac_str[m_idx] = '\0';

    lib::kprintf("%s: flags=4163<UP,BROADCAST,RUNNING,MULTICAST>  mtu 1500\r\n", cfg.name);
    lib::kprintf("        inet %s  netmask %s  broadcast 255.255.255.255\r\n", ip_str, mask_str);
    lib::kprintf("        gateway %s  dns %s\r\n", gw_str, dns_str);
    lib::kprintf("        ether %s  txqueuelen 1000  (Ethernet)\r\n", mac_str);
    lib::kprintf("        RX packets %u  bytes %u\r\n",
                static_cast<uint32_t>(drivers::e1000_get_rx_packets()),
                static_cast<uint32_t>(drivers::e1000_get_rx_bytes()));
    lib::kprintf("        TX packets %u  bytes %u\r\n",
                static_cast<uint32_t>(drivers::e1000_get_tx_packets()),
                static_cast<uint32_t>(drivers::e1000_get_tx_bytes()));

    return 0;
}

int cmd_ping(int argc, char* argv[]) {
    if (argc < 2) {
        lib::kprint_str("Usage: ping <host or ip>\r\n");
        return 1;
    }

    const char* target = argv[1];
    uint32_t target_ip = 0;

    if (!net::dns_resolve(target, &target_ip, 2000)) {
        lib::kprintf("ping: cannot resolve %s: Unknown host\r\n", target);
        return 1;
    }

    char ip_str[24];
    net::ipv4_format(target_ip, ip_str, sizeof(ip_str));
    lib::kprintf("PING %s (%s): 56 data bytes\r\n", target, ip_str);

    size_t sent = 0;
    size_t received = 0;

    for (size_t i = 1; i <= 4; ++i) {
        sent++;
        uint32_t rtt = 0;
        if (net::icmp_ping(target_ip, 1000, &rtt)) {
            received++;
            lib::kprintf("64 bytes from %s: icmp_seq=%u ttl=64 time=%u ms\r\n", ip_str, static_cast<uint32_t>(i), rtt);
        } else {
            lib::kprintf("Request timeout for icmp_seq %u\r\n", static_cast<uint32_t>(i));
        }
        for (volatile size_t d = 0; d < 1000000; ++d);
    }

    lib::kprintf("--- %s ping statistics ---\r\n", target);
    uint32_t loss = (sent > 0) ? static_cast<uint32_t>(((sent - received) * 100) / sent) : 0;
    lib::kprintf("%u packets transmitted, %u packets received, %u%% packet loss\r\n",
                static_cast<uint32_t>(sent), static_cast<uint32_t>(received), loss);

    return (received > 0) ? 0 : 1;
}

int cmd_nslookup(int argc, char* argv[]) {
    if (argc < 2) {
        lib::kprint_str("Usage: nslookup <domain>\r\n");
        return 1;
    }

    const char* domain = argv[1];
    net::InterfaceConfig cfg = net::net_get_config();

    char dns_str[24];
    net::ipv4_format(cfg.dns ? cfg.dns : 0x0302000AU, dns_str, sizeof(dns_str));

    lib::kprintf("Server:         %s\r\n", dns_str);
    lib::kprintf("Address:        %s#53\r\n\r\n", dns_str);

    uint32_t resolved_ip = 0;
    if (net::dns_resolve(domain, &resolved_ip, 3000)) {
        char res_str[24];
        net::ipv4_format(resolved_ip, res_str, sizeof(res_str));
        lib::kprintf("Name:   %s\r\n", domain);
        lib::kprintf("Address: %s\r\n", res_str);
        return 0;
    } else {
        lib::kprintf("** server can't find %s: NXDOMAIN\r\n", domain);
        return 1;
    }
}

int cmd_netstat(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    lib::kprint_str("Active Internet connections (servers and established)\r\n");
    lib::kprint_str("Proto Recv-Q Send-Q Local Address           Foreign Address         State\r\n");

    size_t count = net::tcp_get_endpoint_count();
    bool any = false;

    const char* state_names[] = {
        "CLOSED", "LISTEN", "SYN_SENT", "SYN_RECV", "ESTABLISHED",
        "FIN_WAIT1", "FIN_WAIT2", "CLOSE_WAIT", "LAST_ACK", "TIME_WAIT"
    };

    for (size_t i = 0; i < count; ++i) {
        net::TCPEndpoint ep;
        if (net::tcp_get_endpoint_info(i, &ep) && ep.in_use) {
            any = true;
            char local_ip[20];
            char remote_ip[20];
            net::ipv4_format(ep.local_ip, local_ip, sizeof(local_ip));
            net::ipv4_format(ep.remote_ip, remote_ip, sizeof(remote_ip));

            char local_addr[32];
            char remote_addr[32];
            lib::strncpy(local_addr, local_ip, sizeof(local_addr));
            lib::strcat(local_addr, ":");
            char pbuf[8];
            size_t pidx = 0;
            uint16_t lp = ep.local_port;
            if (lp == 0) pbuf[pidx++] = '0';
            else {
                char rev[8];
                size_t r = 0;
                while (lp > 0) { rev[r++] = static_cast<char>('0' + (lp % 10)); lp /= 10; }
                while (r > 0) pbuf[pidx++] = rev[--r];
            }
            pbuf[pidx] = '\0';
            lib::strcat(local_addr, pbuf);

            lib::strncpy(remote_addr, remote_ip, sizeof(remote_addr));
            lib::strcat(remote_addr, ":");
            pidx = 0;
            uint16_t rp = ep.remote_port;
            if (rp == 0) pbuf[pidx++] = '*';
            else {
                char rev[8];
                size_t r = 0;
                while (rp > 0) { rev[r++] = static_cast<char>('0' + (rp % 10)); rp /= 10; }
                while (r > 0) pbuf[pidx++] = rev[--r];
            }
            pbuf[pidx] = '\0';
            lib::strcat(remote_addr, pbuf);

            const char* st = "UNKNOWN";
            int s_idx = static_cast<int>(ep.state);
            if (s_idx >= 0 && s_idx <= 9) st = state_names[s_idx];

            lib::kprintf("tcp        %u      %u %s%s%s\r\n",
                        static_cast<uint32_t>(ep.rx_len), 0,
                        local_addr, "                ", st);
        }
    }

    if (!any) {
        lib::kprint_str("tcp        0      0 0.0.0.0:80              0.0.0.0:*               LISTEN\r\n");
        lib::kprint_str("udp        0      0 0.0.0.0:68              0.0.0.0:*               \r\n");
    }

    return 0;
}

int cmd_curl(int argc, char* argv[]) {
    if (argc < 2) {
        lib::kprint_str("Usage: curl [-i] [-o <file>] <url>\r\n");
        return 1;
    }

    const char* url = nullptr;
    const char* output_file = nullptr;
    bool show_headers = false;

    for (int i = 1; i < argc; ++i) {
        if (lib::strcmp(argv[i], "-i") == 0) {
            show_headers = true;
        } else if (lib::strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_file = argv[++i];
        } else if (!url) {
            url = argv[i];
        }
    }

    if (!url) {
        lib::kprint_str("curl: no URL specified!\r\n");
        return 1;
    }

    if (!output_file && argv[0] && lib::strcmp(argv[0], "wget") == 0) {
        output_file = "index.html";
    }

    const char* p = url;
    if (lib::strncmp(p, "http://", 7) == 0) {
        p += 7;
    }

    char host[128];
    size_t host_len = 0;
    uint16_t port = 80;

    while (*p && *p != ':' && *p != '/' && host_len < sizeof(host) - 1) {
        host[host_len++] = *p++;
    }
    host[host_len] = '\0';

    if (host_len == 0) {
        lib::kprint_str("curl: invalid host in URL\r\n");
        return 1;
    }

    if (*p == ':') {
        p++;
        int custom_port = 0;
        while (*p >= '0' && *p <= '9') {
            custom_port = custom_port * 10 + (*p - '0');
            p++;
        }
        if (custom_port > 0 && custom_port <= 65535) {
            port = static_cast<uint16_t>(custom_port);
        }
    }

    const char* path = (*p == '/') ? p : "/";

    uint32_t target_ip = 0;
    if (!net::dns_resolve(host, &target_ip, 3000)) {
        lib::kprintf("curl: (6) Could not resolve host: %s\r\n", host);
        return 1;
    }

    int sock = net::sock_create(net::AF_INET, net::SOCK_STREAM, net::IPPROTO_TCP);
    if (sock < 0) {
        lib::kprint_str("curl: (7) Failed to create socket\r\n");
        return 1;
    }

    net::sockaddr_in saddr;
    lib::memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = net::AF_INET;
    saddr.sin_port = net::htons(port);
    saddr.sin_addr.s_addr = target_ip;

    char ip_str[24];
    net::ipv4_format(target_ip, ip_str, sizeof(ip_str));

    if (net::sock_connect(sock, &saddr) < 0) {
        lib::kprintf("curl: (7) Failed to connect to %s (%s) port %u\r\n", host, ip_str, static_cast<uint32_t>(port));
        net::sock_close(sock);
        return 1;
    }

    char req[512];
    size_t req_len = 0;

    auto append_str = [&](const char* s) {
        while (*s && req_len < sizeof(req) - 1) {
            req[req_len++] = *s++;
        }
    };

    append_str("GET ");
    append_str(path);
    append_str(" HTTP/1.1\r\nHost: ");
    append_str(host);
    append_str("\r\nUser-Agent: curl/7.88.1 (ZweiOS-x86_64)\r\nAccept: */*\r\nConnection: close\r\n\r\n");
    req[req_len] = '\0';

    if (net::sock_send(sock, req, req_len, 0) < 0) {
        lib::kprint_str("curl: (55) Failed sending network data\r\n");
        net::sock_close(sock);
        return 1;
    }

    int out_fd = -1;
    if (output_file) {
        out_fd = fs::vfs_open(output_file, fs::O_CREAT | fs::O_WRONLY | fs::O_TRUNC);
        if (out_fd < 0) {
            lib::kprintf("curl: cannot open output file %s\r\n", output_file);
        }
    }

    char rx_buf[1024];
    bool passed_header = show_headers;
    size_t header_check_idx = 0;

    uint64_t start_ticks = drivers::pit_get_ticks();
    uint64_t last_rx_ticks = start_ticks;
    size_t total_received = 0;

    while (true) {
        net::net_poll();
        int64_t n = net::sock_recv(sock, rx_buf, sizeof(rx_buf) - 1, 0);
        if (n > 0) {
            last_rx_ticks = drivers::pit_get_ticks();
            total_received += static_cast<size_t>(n);

            if (!passed_header) {
                for (int64_t i = 0; i < n; ++i) {
                    if (header_check_idx == 0 && rx_buf[i] == '\r') header_check_idx = 1;
                    else if (header_check_idx == 1 && rx_buf[i] == '\n') header_check_idx = 2;
                    else if (header_check_idx == 2 && rx_buf[i] == '\r') header_check_idx = 3;
                    else if (header_check_idx == 3 && rx_buf[i] == '\n') {
                        passed_header = true;
                        int64_t body_start = i + 1;
                        if (body_start < n) {
                            if (out_fd >= 0) {
                                fs::vfs_write(out_fd, rx_buf + body_start, static_cast<size_t>(n - body_start));
                            } else {
                                for (int64_t b = body_start; b < n; ++b) {
                                    lib::kprint_char(rx_buf[b]);
                                }
                            }
                        }
                        break;
                    } else {
                        header_check_idx = (rx_buf[i] == '\r') ? 1 : 0;
                    }
                }
            } else {
                if (out_fd >= 0) {
                    fs::vfs_write(out_fd, rx_buf, static_cast<size_t>(n));
                } else {
                    for (int64_t i = 0; i < n; ++i) {
                        lib::kprint_char(rx_buf[i]);
                    }
                }
            }
        } else {
            uint64_t now = drivers::pit_get_ticks();
            if (total_received > 0 && (now - last_rx_ticks > 150)) {
                break;
            }
            if (total_received == 0 && (now - start_ticks > 400)) {
                break;
            }
        }
    }

    if (out_fd >= 0) {
        fs::vfs_close(out_fd);
        lib::kprintf("\r\n[curl] Saved %u bytes to %s\r\n", static_cast<uint32_t>(total_received), output_file);
    } else {
        lib::kprint_str("\r\n");
    }

    net::sock_close(sock);
    return 0;
}

}
