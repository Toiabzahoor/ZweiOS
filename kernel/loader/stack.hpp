

#pragma once

#include <stdint.h>
#include <stddef.h>
#include "loader/binary_info.hpp"

namespace loader {


inline constexpr uint64_t AT_NULL          = 0;
inline constexpr uint64_t AT_IGNORE        = 1;
inline constexpr uint64_t AT_EXECFD        = 2;
inline constexpr uint64_t AT_PHDR          = 3;
inline constexpr uint64_t AT_PHENT         = 4;
inline constexpr uint64_t AT_PHNUM         = 5;
inline constexpr uint64_t AT_PAGESZ        = 6;
inline constexpr uint64_t AT_BASE          = 7;
inline constexpr uint64_t AT_FLAGS         = 8;
inline constexpr uint64_t AT_ENTRY         = 9;
inline constexpr uint64_t AT_NOTELF        = 10;
inline constexpr uint64_t AT_UID           = 11;
inline constexpr uint64_t AT_EUID          = 12;
inline constexpr uint64_t AT_GID           = 13;
inline constexpr uint64_t AT_EGID          = 14;
inline constexpr uint64_t AT_PLATFORM      = 15;
inline constexpr uint64_t AT_HWCAP         = 16;
inline constexpr uint64_t AT_CLKTCK        = 17;
inline constexpr uint64_t AT_SECURE        = 23;
inline constexpr uint64_t AT_RANDOM        = 25;
inline constexpr uint64_t AT_EXECFN        = 31;

struct Elf64_auxv_t {
    uint64_t a_type;
    uint64_t a_val;
};



uint64_t setup_system_v_stack(
    uint64_t stack_bottom,
    size_t stack_size,
    const BinaryInfo* info,
    int argc,
    const char* const argv[],
    const char* const envp[] = nullptr
);

}
