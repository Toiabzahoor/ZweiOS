/* ==============================================================================
 * ZweiOS - Bare-Metal x86_64 Operating System
 * Component: System V AMD64 Initial Process Stack Builder Header
 * ============================================================================== */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include "loader/binary_info.hpp"

namespace loader {

// ELF Auxiliary Vector Entry Types (System V AMD64 ABI)
inline constexpr uint64_t AT_NULL          = 0;
inline constexpr uint64_t AT_IGNORE        = 1;
inline constexpr uint64_t AT_EXECFD        = 2;
inline constexpr uint64_t AT_PHDR          = 3;   // Program header table address
inline constexpr uint64_t AT_PHENT         = 4;   // Size of program header entry (56)
inline constexpr uint64_t AT_PHNUM         = 5;   // Number of program headers
inline constexpr uint64_t AT_PAGESZ        = 6;   // System page size (4096)
inline constexpr uint64_t AT_BASE          = 7;   // Base address of interpreter (0 for static)
inline constexpr uint64_t AT_FLAGS         = 8;   // Flags
inline constexpr uint64_t AT_ENTRY         = 9;   // Entry point of program
inline constexpr uint64_t AT_NOTELF        = 10;
inline constexpr uint64_t AT_UID           = 11;  // Real UID
inline constexpr uint64_t AT_EUID          = 12;  // Effective UID
inline constexpr uint64_t AT_GID           = 13;  // Real GID
inline constexpr uint64_t AT_EGID          = 14;  // Effective GID
inline constexpr uint64_t AT_PLATFORM      = 15;
inline constexpr uint64_t AT_HWCAP         = 16;
inline constexpr uint64_t AT_CLKTCK        = 17;  // Frequency of times()
inline constexpr uint64_t AT_SECURE        = 23;  // Secure mode boolean
inline constexpr uint64_t AT_RANDOM        = 25;  // Address of 16 random bytes (canary)
inline constexpr uint64_t AT_EXECFN        = 31;  // Filename of executable

struct Elf64_auxv_t {
    uint64_t a_type;
    uint64_t a_val;
};

// Builds a complete System V ABI process initialization stack.
// Returns the aligned 16-byte user stack pointer (%rsp) ready for enter_user_mode().
uint64_t setup_system_v_stack(
    uint64_t stack_bottom,
    size_t stack_size,
    const BinaryInfo* info,
    int argc,
    const char* const argv[],
    const char* const envp[] = nullptr
);

} // namespace loader
