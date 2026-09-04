/* ==============================================================================
 * ZweiOS - Bare-Metal x86_64 Operating System
 * Component: Binary Container Abstraction & Metadata (ELF64 & PE32+)
 * ============================================================================== */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

namespace loader {

enum class BinaryFormat {
    UNKNOWN = 0,
    ELF64,
    PE32_PLUS
};

enum class BinaryArch {
    UNKNOWN = 0,
    X86_64
};

enum class BinarySubsystem {
    UNKNOWN = 0,
    LINUX_CLI,
    WINDOWS_CUI,
    WINDOWS_GUI
};

// Segment permission flags
inline constexpr uint32_t SEG_FLAG_EXEC  = (1U << 0);
inline constexpr uint32_t SEG_FLAG_WRITE = (1U << 1);
inline constexpr uint32_t SEG_FLAG_READ  = (1U << 2);

struct BinarySegment {
    char name[16];
    uint64_t vaddr;
    uint64_t mem_size;
    uint64_t file_offset;
    uint64_t file_size;
    uint32_t flags;
    uint64_t alignment;
};

inline constexpr size_t MAX_BINARY_SEGMENTS = 16;

struct BinaryInfo {
    BinaryFormat format;
    BinaryArch arch;
    BinarySubsystem subsystem;
    uint64_t entry_point;
    uint64_t image_base;
    uint64_t image_size;
    size_t segment_count;
    BinarySegment segments[MAX_BINARY_SEGMENTS];

    // Linux ELF specific details
    uint64_t phdr_vaddr;
    uint16_t phnum;
    uint16_t phentsize;

    // Windows PE specific details
    uint64_t stack_reserve;
    uint64_t stack_commit;
    uint32_t import_table_rva;
    uint32_t import_table_size;
};

} // namespace loader
