/* ==============================================================================
 * ZweiOS - Bare-Metal x86_64 Operating System
 * Component: Interactive Kernel Shell & Line Editor Implementation
 * ============================================================================== */

#include "shell/shell.hpp"
#include "drivers/keyboard.hpp"
#include "drivers/serial.hpp"
#include "drivers/vga.hpp"
#include "lib/string.hpp"
#include "lib/kprintf.hpp"

namespace shell {

static command_entry_t command_registry[MAX_COMMANDS];
static size_t command_count = 0;

static char line_buf[MAX_LINE_LEN];
static size_t line_len = 0;

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
        // Skip leading whitespace
        while (*ptr == ' ' || *ptr == '\t' || *ptr == '\r' || *ptr == '\n') {
            ptr++;
        }
        if (*ptr == '\0') break;

        // Check for quoted argument
        if (*ptr == '"') {
            ptr++; // Skip opening quote
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

void shell_dispatch(char* line) {
    if (!line) return;

    // Check if line is only whitespace
    bool has_content = false;
    for (size_t i = 0; line[i] != '\0'; ++i) {
        if (!lib::is_space(line[i])) {
            has_content = true;
            break;
        }
    }
    if (!has_content) {
        return; // Empty line
    }

    int argc = 0;
    char* argv[MAX_ARGS];
    tokenize(line, &argc, argv);

    if (argc == 0 || !argv[0]) return;

    for (size_t i = 0; i < command_count; ++i) {
        if (lib::strcmp(argv[0], command_registry[i].name) == 0) {
            command_registry[i].handler(argc, argv);
            return;
        }
    }

    lib::kprint_str("Unknown command: '");
    lib::kprint_str(argv[0]);
    lib::kprint_str("'. Type 'help' for available commands.\r\n");
}

void shell_feed_char(char c) {
    if (c == '\x1b') { // Escape key: clear line
        while (line_len > 0) {
            line_len--;
            drivers::serial_puts("\b \b");
            drivers::vga_putc('\b');
        }
        line_buf[0] = '\0';
        return;
    }

    if (c == '\x03') { // Ctrl+C
        line_len = 0;
        line_buf[0] = '\0';
        drivers::serial_puts("^C\r\nzwei> ");
        drivers::vga_puts("^C\n\rzwei> ");
        return;
    }

    if (c == '\x0c') { // Ctrl+L
        drivers::vga_clear();
        drivers::serial_puts("\033[2J\033[H");
        drivers::serial_puts("zwei> ");
        drivers::vga_puts("zwei> ");
        line_buf[line_len] = '\0';
        drivers::serial_puts(line_buf);
        drivers::vga_puts(line_buf);
        return;
    }

    if (c == '\b' || c == '\x7f') { // Backspace
        if (line_len > 0) {
            line_len--;
            line_buf[line_len] = '\0';
            drivers::serial_puts("\b \b");
            drivers::vga_putc('\b');
        }
        return;
    }

    if (c == '\r' || c == '\n') { // Enter
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

    // Normal printable character
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
    shell_register_command("disks",   "Display ATA hard drive controller status", cmd_disks);
    shell_register_command("panic",   "Trigger a kernel panic exception test", cmd_panic);

    drivers::serial_puts("[SHELL] Interactive kernel shell started - made by toiabzahoor.\r\n");
    drivers::vga_puts("[SHELL] Interactive kernel shell started - made by toiabzahoor.\n\r");
    drivers::serial_puts("zwei> ");
    drivers::vga_puts("zwei> ");
}

void shell_run() {
    // Enable CPU interrupts so hardware IRQs (keyboard, timer) can fire
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

} // namespace shell
