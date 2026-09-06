

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "loader/binary_info.hpp"

namespace loader {

inline constexpr uint16_t IMAGE_DOS_SIGNATURE = 0x5A4D;
inline constexpr uint32_t IMAGE_NT_SIGNATURE  = 0x00004550;

inline constexpr uint16_t IMAGE_FILE_MACHINE_AMD64 = 0x8664;
inline constexpr uint16_t IMAGE_NT_OPTIONAL_HDR64_MAGIC = 0x020B;

inline constexpr uint16_t IMAGE_SUBSYSTEM_WINDOWS_GUI = 2;
inline constexpr uint16_t IMAGE_SUBSYSTEM_WINDOWS_CUI = 3;

inline constexpr size_t IMAGE_DIRECTORY_ENTRY_EXPORT = 0;
inline constexpr size_t IMAGE_DIRECTORY_ENTRY_IMPORT = 1;
inline constexpr size_t IMAGE_DIRECTORY_ENTRY_BASERELOC = 5;

inline constexpr uint32_t IMAGE_SCN_MEM_EXECUTE = 0x20000000;
inline constexpr uint32_t IMAGE_SCN_MEM_READ    = 0x40000000;
inline constexpr uint32_t IMAGE_SCN_MEM_WRITE   = 0x80000000;

struct ImageDosHeader {
    uint16_t e_magic;
    uint16_t e_cblp;
    uint16_t e_cp;
    uint16_t e_crlc;
    uint16_t e_cparhdr;
    uint16_t e_minalloc;
    uint16_t e_maxalloc;
    uint16_t e_ss;
    uint16_t e_sp;
    uint16_t e_csum;
    uint16_t e_ip;
    uint16_t e_cs;
    uint16_t e_lfarlc;
    uint16_t e_ovno;
    uint16_t e_res[4];
    uint16_t e_oemid;
    uint16_t e_oeminfo;
    uint16_t e_res2[10];
    int32_t  e_lfanew;
};

struct ImageFileHeader {
    uint16_t Machine;
    uint16_t NumberOfSections;
    uint32_t TimeDateStamp;
    uint32_t PointerToSymbolTable;
    uint32_t NumberOfSymbols;
    uint16_t SizeOfOptionalHeader;
    uint16_t Characteristics;
};

struct ImageDataDirectory {
    uint32_t VirtualAddress;
    uint32_t Size;
};

struct ImageOptionalHeader64 {
    uint16_t Magic;
    uint8_t  MajorLinkerVersion;
    uint8_t  MinorLinkerVersion;
    uint32_t SizeOfCode;
    uint32_t SizeOfInitializedData;
    uint32_t SizeOfUninitializedData;
    uint32_t AddressOfEntryPoint;
    uint32_t BaseOfCode;
    uint64_t ImageBase;
    uint32_t SectionAlignment;
    uint32_t FileAlignment;
    uint16_t MajorOperatingSystemVersion;
    uint16_t MinorOperatingSystemVersion;
    uint16_t MajorImageVersion;
    uint16_t MinorImageVersion;
    uint16_t MajorSubsystemVersion;
    uint16_t MinorSubsystemVersion;
    uint32_t Win32VersionValue;
    uint32_t SizeOfImage;
    uint32_t SizeOfHeaders;
    uint32_t CheckSum;
    uint16_t Subsystem;
    uint16_t DllCharacteristics;
    uint64_t SizeOfStackReserve;
    uint64_t SizeOfStackCommit;
    uint64_t SizeOfHeapReserve;
    uint64_t SizeOfHeapCommit;
    uint32_t LoaderFlags;
    uint32_t NumberOfRvaAndSizes;
    ImageDataDirectory DataDirectory[16];
};

struct ImageSectionHeader {
    uint8_t  Name[8];
    uint32_t VirtualSize;
    uint32_t VirtualAddress;
    uint32_t SizeOfRawData;
    uint32_t PointerToRawData;
    uint32_t PointerToRelocations;
    uint32_t PointerToLinenumbers;
    uint16_t NumberOfRelocations;
    uint16_t NumberOfLinenumbers;
    uint32_t Characteristics;
};

struct ImageImportDescriptor {
    union {
        uint32_t Characteristics;
        uint32_t OriginalFirstThunk;
    };
    uint32_t TimeDateStamp;
    uint32_t ForwarderChain;
    uint32_t Name;
    uint32_t FirstThunk;
};

struct ImageImportByName {
    uint16_t Hint;
    char Name[1];
};

inline constexpr uint64_t IMAGE_ORDINAL_FLAG64 = 0x8000000000000000ULL;

bool pe_probe(const uint8_t* data, size_t size);
bool pe_parse(const uint8_t* data, size_t size, BinaryInfo* out_info);
bool pe_resolve_imports(uint64_t image_base, const BinaryInfo* info);

}

