#include "shell/shell.hpp"
#include "drivers/keyboard.hpp"
#include "drivers/serial.hpp"
#include "drivers/vga.hpp"
#include "lib/string.hpp"
#include "lib/kprintf.hpp"
#include "loader/loader.hpp"
#include "fs/vfs.hpp"
#include "toolchain/compiler.hpp"

namespace shell {

static command_entry_t command_registry[MAX_COMMANDS];
static size_t command_count = 0;

static char line_buf[MAX_LINE_LEN];
static size_t line_len = 0;

inline constexpr size_t HISTORY_MAX = 16;
static char history_entries[HISTORY_MAX][MAX_LINE_LEN];
static size_t history_count = 0;
static int history_view_idx = -1;

static int ansi_state = 0;

void shell_register_command(const char* name, const char* description, command_fn handler) {
    if (command_count < MAX_COMMANDS) {
        command_registry[command_count].name = name;
        command_registry[command_count].description = description;
        command_registry[command_count].handler = handler;
        command_count++;
    }
}

static void tokenize(char* line, int* argc_out, char* argv_out[MAX_ARGS]) {
    int argc = 0;
    char* ptr = line;

    while (*ptr != '\0' && argc < static_cast<int>(MAX_ARGS) - 1) {
        while (*ptr == ' ' || *ptr == '\t' || *ptr == '\r' || *ptr == '\n') {
            ptr++;
        }
        if (*ptr == '\0') break;

        if (*ptr == '"') {
            ptr++;
            argv_out[argc++] = ptr;
            while (*ptr != '\0' && *ptr != '"') {
                ptr++;
            }
            if (*ptr == '"') {
                *ptr = '\0';
                ptr++;
            }
        } else {
            argv_out[argc++] = ptr;
            while (*ptr != '\0' && *ptr != ' ' && *ptr != '\t' && *ptr != '\r' && *ptr != '\n') {
                ptr++;
            }
            if (*ptr != '\0') {
                *ptr = '\0';
                ptr++;
            }
        }
    }
    argv_out[argc] = nullptr;
    *argc_out = argc;
}

static bool try_direct_execution(const char* target_path, int argc, char* argv[]) {
    fs::VNodeStat st;
    if (fs::vfs_stat(target_path, &st) == 0 && st.type == fs::VNodeType::FILE && st.size > 0) {
        int64_t code = loader::loader_execute_path(target_path, argc, const_cast<const char**>(argv));
        (void)code;
        return true;
    }
    return false;
}

void shell_dispatch(char* line) {
    if (!line) return;

    bool has_content = false;
    for (size_t i = 0; line[i] != '\0'; ++i) {
        if (!lib::is_space(line[i])) {
            has_content = true;
            break;
        }
    }
    if (!has_content) return;

    if (history_count < HISTORY_MAX) {
        lib::strncpy(history_entries[history_count], line, MAX_LINE_LEN);
        history_count++;
    } else {
        for (size_t i = 0; i < HISTORY_MAX - 1; ++i) {
            lib::memcpy(history_entries[i], history_entries[i + 1], MAX_LINE_LEN);
        }
        lib::strncpy(history_entries[HISTORY_MAX - 1], line, MAX_LINE_LEN);
    }
    history_view_idx = -1;

    int argc = 0;
    char* argv[MAX_ARGS];
    tokenize(line, &argc, argv);

    if (argc == 0) return;

    for (size_t i = 0; i < command_count; ++i) {
        if (lib::strcmp(command_registry[i].name, argv[0]) == 0) {
            command_registry[i].handler(argc, argv);
            return;
        }
    }

    if (try_direct_execution(argv[0], argc, argv)) {
        return;
    }

    char bin_path[128];
    lib::strncpy(bin_path, "/bin/", sizeof(bin_path));
    lib::strcat(bin_path, argv[0]);
    if (try_direct_execution(bin_path, argc, argv)) {
        return;
    }

    char disk_bin_path[128];
    lib::strncpy(disk_bin_path, "/mnt/disk0/bin/", sizeof(disk_bin_path));
    lib::strcat(disk_bin_path, argv[0]);
    if (try_direct_execution(disk_bin_path, argc, argv)) {
        return;
    }

    lib::kprint_str("Unknown command: '");
    lib::kprint_str(argv[0]);
    lib::kprint_str("'. Type 'help' for available commands.\r\n");
}

static void handle_autocomplete() {
    if (line_len == 0) return;

    size_t last_space = 0;
    bool found_space = false;
    for (size_t i = 0; i < line_len; ++i) {
        if (line_buf[i] == ' ') {
            last_space = i + 1;
            found_space = true;
        }
    }

    const char* prefix = line_buf + last_space;
    size_t prefix_len = line_len - last_space;
    if (prefix_len == 0) return;

    if (!found_space) {
        const char* match = nullptr;
        size_t match_count = 0;

        for (size_t i = 0; i < command_count; ++i) {
            if (lib::strncmp(command_registry[i].name, prefix, prefix_len) == 0) {
                match = command_registry[i].name;
                match_count++;
            }
        }

        if (match_count == 1 && match) {
            size_t match_len = lib::strlen(match);
            for (size_t i = prefix_len; i < match_len && line_len < MAX_LINE_LEN - 2; ++i) {
                char c = match[i];
                line_buf[line_len++] = c;
                drivers::serial_putc(c);
                drivers::vga_putc(c);
            }
            line_buf[line_len++] = ' ';
            line_buf[line_len] = '\0';
            drivers::serial_putc(' ');
            drivers::vga_putc(' ');
        } else if (match_count > 1) {
            drivers::serial_puts("\r\n");
            drivers::vga_puts("\r\n");
            for (size_t i = 0; i < command_count; ++i) {
                if (lib::strncmp(command_registry[i].name, prefix, prefix_len) == 0) {
                    drivers::serial_puts(command_registry[i].name);
                    drivers::serial_puts("  ");
                    drivers::vga_puts(command_registry[i].name);
                    drivers::vga_puts("  ");
                }
            }
            drivers::serial_puts("\r\nzwei> ");
            drivers::vga_puts("\n\rzwei> ");
            line_buf[line_len] = '\0';
            drivers::serial_puts(line_buf);
            drivers::vga_puts(line_buf);
        }
    } else {
        fs::VNode* root = fs::vfs_get_root();
        if (!root || !root->ops || !root->ops->readdir) return;

        char name[64];
        fs::VNodeType type;
        size_t sz = 0;
        size_t idx = 0;
        const char* match = nullptr;
        size_t match_count = 0;
        char matched_name[64];

        while (root->ops->readdir(root, idx++, name, &type, &sz) > 0) {
            if (lib::strncmp(name, prefix, prefix_len) == 0) {
                lib::strncpy(matched_name, name, sizeof(matched_name));
                match = matched_name;
                match_count++;
            }
        }

        if (match_count == 1 && match) {
            size_t m_len = lib::strlen(match);
            for (size_t i = prefix_len; i < m_len && line_len < MAX_LINE_LEN - 2; ++i) {
                char c = match[i];
                line_buf[line_len++] = c;
                drivers::serial_putc(c);
                drivers::vga_putc(c);
            }
            line_buf[line_len] = '\0';
        }
    }
}

static void handle_history_up() {
    if (history_count == 0) return;

    if (history_view_idx == -1) {
        history_view_idx = static_cast<int>(history_count) - 1;
    } else if (history_view_idx > 0) {
        history_view_idx--;
    }

    while (line_len > 0) {
        drivers::serial_puts("\b \b");
        drivers::vga_putc('\b');
        line_len--;
    }

    lib::strncpy(line_buf, history_entries[history_view_idx], MAX_LINE_LEN);
    line_len = lib::strlen(line_buf);
    drivers::serial_puts(line_buf);
    drivers::vga_puts(line_buf);
}

static void handle_history_down() {
    if (history_count == 0 || history_view_idx == -1) return;

    if (history_view_idx < static_cast<int>(history_count) - 1) {
        history_view_idx++;
        while (line_len > 0) {
            drivers::serial_puts("\b \b");
            drivers::vga_putc('\b');
            line_len--;
        }
        lib::strncpy(line_buf, history_entries[history_view_idx], MAX_LINE_LEN);
        line_len = lib::strlen(line_buf);
        drivers::serial_puts(line_buf);
        drivers::vga_puts(line_buf);
    } else {
        history_view_idx = -1;
        while (line_len > 0) {
            drivers::serial_puts("\b \b");
            drivers::vga_putc('\b');
            line_len--;
        }
        line_buf[0] = '\0';
    }
}

void shell_feed_char(char c) {
    if (ansi_state == 0) {
        if (c == '\033') {
            ansi_state = 1;
            return;
        }
    } else if (ansi_state == 1) {
        if (c == '[') {
            ansi_state = 2;
            return;
        }
        ansi_state = 0;
    } else if (ansi_state == 2) {
        ansi_state = 0;
        if (c == 'A') {
            handle_history_up();
            return;
        } else if (c == 'B') {
            handle_history_down();
            return;
        }
        return;
    }

    if (c == '\t') {
        handle_autocomplete();
        return;
    }

    if (c == '\x03') {
        line_len = 0;
        line_buf[0] = '\0';
        history_view_idx = -1;
        drivers::serial_puts("^C\r\nzwei> ");
        drivers::vga_puts("^C\n\rzwei> ");
        return;
    }

    if (c == '\x0c') {
        drivers::vga_clear();
        drivers::serial_puts("\033[2J\033[H");
        drivers::serial_puts("zwei> ");
        drivers::vga_puts("zwei> ");
        line_buf[line_len] = '\0';
        drivers::serial_puts(line_buf);
        drivers::vga_puts(line_buf);
        return;
    }

    if (c == '\b' || c == '\x7f') {
        if (line_len > 0) {
            line_len--;
            line_buf[line_len] = '\0';
            drivers::serial_puts("\b \b");
            drivers::vga_putc('\b');
        }
        return;
    }

    if (c == '\r' || c == '\n') {
        drivers::serial_puts("\r\n");
        drivers::vga_puts("\r\n");
        line_buf[line_len] = '\0';
        shell_dispatch(line_buf);
        line_len = 0;
        line_buf[0] = '\0';
        drivers::serial_puts("zwei> ");
        drivers::vga_puts("zwei> ");
        return;
    }

    if (line_len < MAX_LINE_LEN - 1) {
        line_buf[line_len++] = c;
        line_buf[line_len] = '\0';
        drivers::serial_putc(c);
        drivers::vga_putc(c);
    }
}

void shell_init() {
    command_count = 0;
    line_len = 0;
    line_buf[0] = '\0';
    history_count = 0;
    history_view_idx = -1;
    ansi_state = 0;

    shell_register_command("help",    "Display this list of commands", cmd_help);
    shell_register_command("version", "Display OS version and author watermark", cmd_version);
    shell_register_command("about",   "Display OS version and author watermark", cmd_about);
    shell_register_command("date",    "Display current hardware RTC calendar date", cmd_date);
    shell_register_command("time",    "Display current hardware RTC wall-clock time", cmd_time);
    shell_register_command("uptime",  "Display system uptime since kernel boot", cmd_uptime);
    shell_register_command("sleep",   "Suspend shell execution for integer seconds", cmd_sleep);
    shell_register_command("mem",     "Display physical and heap memory statistics", cmd_mem);
    shell_register_command("cpu",     "Display CPUID vendor, model, and feature flags", cmd_cpu);
    shell_register_command("clear",   "Clear the console screen", cmd_clear);
    shell_register_command("echo",    "Print arguments to output", cmd_echo);
    shell_register_command("bininfo", "Inspect Linux ELF64 and Windows PE32+ binaries", cmd_bininfo);
    shell_register_command("run",     "Execute Linux ELF64 and Windows PE32+ binaries", cmd_run);
    shell_register_command("ring3",   "Execute Ring 3 user mode syscall test", cmd_ring3);
    shell_register_command("ls",      "List directory contents (POSIX / and Windows C:\\)", cmd_ls);
    shell_register_command("cat",     "Display text file content", cmd_cat);
    shell_register_command("touch",   "Create an empty file", cmd_touch);
    shell_register_command("mkdir",   "Create a directory", cmd_mkdir);
    shell_register_command("rm",      "Delete a file or directory", cmd_rm);
    shell_register_command("write",   "Write or append text to a file", cmd_write);
    shell_register_command("disks",   "Display ATA hard drives and partition table status", cmd_disks);
    shell_register_command("mount",   "Display or attach filesystem mount points", cmd_mount);
    shell_register_command("unmount", "Detach a filesystem mount point", cmd_unmount);
    shell_register_command("ps",         "Display active processes and multitasking status", cmd_ps);
    shell_register_command("kill",       "Terminate a process by PID", cmd_kill);
    shell_register_command("spawn",      "Spawn a program into the process table", cmd_spawn);
    shell_register_command("gui",        "Launch linear framebuffer desktop window manager", cmd_gui);
    shell_register_command("resolution", "Query or configure VBE graphics resolution mode", cmd_resolution);
    shell_register_command("panic",      "Trigger a kernel panic exception test", cmd_panic);
    shell_register_command("gcc",        "Compile C source to native ELF64 executable", toolchain::cmd_gcc);
    shell_register_command("g++",        "Compile C++ source to native ELF64 executable", toolchain::cmd_gpp);
    shell_register_command("tcc",        "Fast Tiny C Compiler for native ELF64 execution", toolchain::cmd_tcc);
    shell_register_command("make",       "Execute Makefile build rules and targets", toolchain::cmd_make);
    shell_register_command("ifconfig",   "Display network interface configuration and statistics", cmd_ifconfig);
    shell_register_command("ping",       "Send ICMP Echo requests to network host", cmd_ping);
    shell_register_command("nslookup",   "Query Internet domain name servers", cmd_nslookup);
    shell_register_command("netstat",    "Display active network connections and sockets", cmd_netstat);
    shell_register_command("curl",       "Transfer data from or to a server (HTTP)", cmd_curl);
    shell_register_command("wget",       "Download files over HTTP", cmd_curl);

    drivers::serial_puts("[SHELL] Interactive kernel shell started - made by toiabzahoor.\r\n");
    drivers::vga_puts("[SHELL] Interactive kernel shell started - made by toiabzahoor.\n\r");
    drivers::serial_puts("zwei> ");
    drivers::vga_puts("zwei> ");
}

void shell_run() {
    asm volatile("sti");

    while (true) {
        char c = 0;
        if (drivers::sys_try_getc(&c)) {
            shell_feed_char(c);
        } else {
            asm volatile("pause");
        }
    }
}

}
