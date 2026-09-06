

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "loader/binary_info.hpp"

namespace loader {


inline constexpr uint8_t ELFMAG0 = 0x7F;
inline constexpr uint8_t ELFMAG1 = 'E';
inline constexpr uint8_t ELFMAG2 = 'L';
inline constexpr uint8_t ELFMAG3 = 'F';

inline constexpr size_t EI_NIDENT  = 16;
inline constexpr size_t EI_CLASS   = 4;
inline constexpr size_t EI_DATA    = 5;
inline constexpr size_t EI_VERSION = 6;
inline constexpr size_t EI_OSABI   = 7;

inline constexpr uint8_t ELFCLASS64  = 2;
inline constexpr uint8_t ELFDATA2LSB = 1;
inline constexpr uint8_t EV_CURRENT  = 1;


inline constexpr uint16_t ET_NONE = 0;
inline constexpr uint16_t ET_REL  = 1;
inline constexpr uint16_t ET_EXEC = 2;
inline constexpr uint16_t ET_DYN  = 3;


inline constexpr uint16_t EM_X86_64 = 62;


inline constexpr uint32_t PT_NULL    = 0;
inline constexpr uint32_t PT_LOAD    = 1;
inline constexpr uint32_t PT_DYNAMIC = 2;
inline constexpr uint32_t PT_INTERP  = 3;
inline constexpr uint32_t PT_NOTE    = 4;
inline constexpr uint32_t PT_SHLIB   = 5;
inline constexpr uint32_t PT_PHDR    = 6;
inline constexpr uint32_t PT_TLS     = 7;


inline constexpr uint32_t PF_X = 1;
inline constexpr uint32_t PF_W = 2;
inline constexpr uint32_t PF_R = 4;

struct Elf64_Ehdr {
    uint8_t  e_ident[EI_NIDENT];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

struct Elf64_Phdr {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
};

bool elf_probe(const uint8_t* data, size_t size);
bool elf_parse(const uint8_t* data, size_t size, BinaryInfo* out_info);

}
