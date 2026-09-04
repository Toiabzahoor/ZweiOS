/* ==============================================================================
 * ZweiOS - Bare-Metal x86_64 Operating System
 * Component: Interactive Kernel Shell & Line Editor Header
 * ============================================================================== */

#pragma once

#include <stdint.h>
#include <stddef.h>

namespace shell {

inline constexpr size_t MAX_LINE_LEN = 256;
inline constexpr size_t MAX_ARGS     = 16;
inline constexpr size_t MAX_COMMANDS = 48;

typedef int (*command_fn)(int argc, char* argv[]);

struct command_entry_t {
    const char* name;
    const char* description;
    command_fn  handler;
};

void shell_init();
void shell_register_command(const char* name, const char* description, command_fn handler);
void shell_dispatch(char* line);
void shell_feed_char(char c);
void shell_run();

// Built-in command handlers
int cmd_help(int argc, char* argv[]);
int cmd_version(int argc, char* argv[]);
int cmd_about(int argc, char* argv[]);
int cmd_date(int argc, char* argv[]);
int cmd_time(int argc, char* argv[]);
int cmd_uptime(int argc, char* argv[]);
int cmd_sleep(int argc, char* argv[]);
int cmd_mem(int argc, char* argv[]);
int cmd_cpu(int argc, char* argv[]);
int cmd_clear(int argc, char* argv[]);
int cmd_echo(int argc, char* argv[]);
int cmd_panic(int argc, char* argv[]);
int cmd_bininfo(int argc, char* argv[]);
int cmd_run(int argc, char* argv[]);
int cmd_ring3(int argc, char* argv[]);
int cmd_ls(int argc, char* argv[]);
int cmd_cat(int argc, char* argv[]);
int cmd_touch(int argc, char* argv[]);
int cmd_mkdir(int argc, char* argv[]);
int cmd_rm(int argc, char* argv[]);
int cmd_write(int argc, char* argv[]);
int cmd_disks(int argc, char* argv[]);

} // namespace shell

using shell::shell_init;
using shell::shell_register_command;
using shell::shell_dispatch;
using shell::shell_feed_char;
using shell::shell_run;
