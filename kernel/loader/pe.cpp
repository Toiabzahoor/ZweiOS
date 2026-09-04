/* ==============================================================================
 * ZweiOS - Bare-Metal x86_64 Operating System
 * Component: Windows 64-bit PE32+ (Portable Executable) Implementation
 * ============================================================================== */

#include "loader/pe.hpp"
#include "win32/win32.hpp"
#include "lib/kprintf.hpp"
#include "lib/string.hpp"

namespace loader {

bool pe_probe(const uint8_t* data, size_t size) {
    if (!data || size < sizeof(ImageDosHeader)) {
        return false;
    }

    const auto* dos = reinterpret_cast<const ImageDosHeader*>(data);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return false;
    }

    if (dos->e_lfanew <= 0) {
        return false;
    }

    size_t pe_offset = static_cast<size_t>(dos->e_lfanew);
    size_t min_pe_size = pe_offset + sizeof(uint32_t) + sizeof(ImageFileHeader) + sizeof(ImageOptionalHeader64);
    if (min_pe_size > size) {
        return false;
    }

    uint32_t pe_sig = *reinterpret_cast<const uint32_t*>(data + pe_offset);
    if (pe_sig != IMAGE_NT_SIGNATURE) {
        return false;
    }

    const auto* file_hdr = reinterpret_cast<const ImageFileHeader*>(data + pe_offset + sizeof(uint32_t));
    if (file_hdr->Machine != IMAGE_FILE_MACHINE_AMD64) {
        return false;
    }

    if (file_hdr->SizeOfOptionalHeader < sizeof(ImageOptionalHeader64)) {
        return false;
    }

    const auto* opt_hdr = reinterpret_cast<const ImageOptionalHeader64*>(
        data + pe_offset + sizeof(uint32_t) + sizeof(ImageFileHeader)
    );

    if (opt_hdr->Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        return false;
    }

    return true;
}

bool pe_parse(const uint8_t* data, size_t size, BinaryInfo* out_info) {
    if (!pe_probe(data, size) || !out_info) {
        return false;
    }

    const auto* dos = reinterpret_cast<const ImageDosHeader*>(data);
    size_t pe_offset = static_cast<size_t>(dos->e_lfanew);

    const auto* file_hdr = reinterpret_cast<const ImageFileHeader*>(data + pe_offset + sizeof(uint32_t));
    const auto* opt_hdr = reinterpret_cast<const ImageOptionalHeader64*>(
        data + pe_offset + sizeof(uint32_t) + sizeof(ImageFileHeader)
    );

    lib::memset(out_info, 0, sizeof(BinaryInfo));
    out_info->format = BinaryFormat::PE32_PLUS;
    out_info->arch = BinaryArch::X86_64;

    if (opt_hdr->Subsystem == IMAGE_SUBSYSTEM_WINDOWS_CUI) {
        out_info->subsystem = BinarySubsystem::WINDOWS_CUI;
    } else if (opt_hdr->Subsystem == IMAGE_SUBSYSTEM_WINDOWS_GUI) {
        out_info->subsystem = BinarySubsystem::WINDOWS_GUI;
    } else {
        out_info->subsystem = BinarySubsystem::UNKNOWN;
    }

    out_info->image_base = opt_hdr->ImageBase;
    out_info->entry_point = opt_hdr->ImageBase + opt_hdr->AddressOfEntryPoint;
    out_info->image_size = opt_hdr->SizeOfImage;
    out_info->stack_reserve = opt_hdr->SizeOfStackReserve;
    out_info->stack_commit = opt_hdr->SizeOfStackCommit;

    if (opt_hdr->NumberOfRvaAndSizes > IMAGE_DIRECTORY_ENTRY_IMPORT) {
        out_info->import_table_rva = opt_hdr->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
        out_info->import_table_size = opt_hdr->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size;
    }

    size_t sections_offset = pe_offset + sizeof(uint32_t) + sizeof(ImageFileHeader) + file_hdr->SizeOfOptionalHeader;
    size_t required_sec_size = sections_offset + (static_cast<size_t>(file_hdr->NumberOfSections) * sizeof(ImageSectionHeader));
    if (required_sec_size > size) {
        return false;
    }

    const auto* sections = reinterpret_cast<const ImageSectionHeader*>(data + sections_offset);
    size_t seg_idx = 0;

    for (uint16_t i = 0; i < file_hdr->NumberOfSections; ++i) {
        if (seg_idx >= MAX_BINARY_SEGMENTS) {
            break;
        }

        const auto* sec = &sections[i];
        BinarySegment* seg = &out_info->segments[seg_idx];

        for (size_t n = 0; n < 8; ++n) {
            seg->name[n] = static_cast<char>(sec->Name[n]);
            if (sec->Name[n] == 0) break;
        }
        seg->name[8] = '\0';

        seg->vaddr = opt_hdr->ImageBase + sec->VirtualAddress;
        seg->mem_size = (sec->VirtualSize > 0) ? sec->VirtualSize : sec->SizeOfRawData;
        seg->file_offset = sec->PointerToRawData;
        seg->file_size = sec->SizeOfRawData;
        seg->alignment = opt_hdr->SectionAlignment;
        seg->flags = 0;

        if (sec->Characteristics & IMAGE_SCN_MEM_READ) {
            seg->flags |= SEG_FLAG_READ;
        }
        if (sec->Characteristics & IMAGE_SCN_MEM_WRITE) {
            seg->flags |= SEG_FLAG_WRITE;
        }
        if (sec->Characteristics & IMAGE_SCN_MEM_EXECUTE) {
            seg->flags |= SEG_FLAG_EXEC;
        }

        seg_idx++;
    }

    out_info->segment_count = seg_idx;
    return true;
}

bool pe_resolve_imports(uint64_t image_base, const BinaryInfo* info) {
    if (!info || info->import_table_rva == 0 || info->import_table_size == 0) {
        return true;
    }

    const uint8_t* base = reinterpret_cast<const uint8_t*>(image_base);
    const auto* desc = reinterpret_cast<const ImageImportDescriptor*>(base + info->import_table_rva);

    while (desc->Name != 0 && desc->FirstThunk != 0) {
        const char* dll_name = reinterpret_cast<const char*>(base + desc->Name);
        uint32_t ilt_rva = desc->OriginalFirstThunk ? desc->OriginalFirstThunk : desc->FirstThunk;
        uint32_t iat_rva = desc->FirstThunk;

        const auto* ilt = reinterpret_cast<const uint64_t*>(base + ilt_rva);
        auto* iat = reinterpret_cast<uint64_t*>(const_cast<uint8_t*>(base) + iat_rva);

        size_t idx = 0;
        while (ilt[idx] != 0) {
            uint64_t entry = ilt[idx];
            uint64_t resolved_addr = 0;
            const char* func_name = nullptr;
            uint16_t ordinal = 0;

            if (entry & IMAGE_ORDINAL_FLAG64) {
                ordinal = static_cast<uint16_t>(entry & 0xFFFF);
                resolved_addr = win32::win32_resolve_symbol(dll_name, nullptr, ordinal);
            } else {
                const auto* ibm = reinterpret_cast<const ImageImportByName*>(base + entry);
                func_name = ibm->Name;
                resolved_addr = win32::win32_resolve_symbol(dll_name, func_name, 0);
            }

            if (resolved_addr != 0) {
                lib::kprint_str("[WIN32] Binding ");
                lib::kprint_str(dll_name);
                lib::kprint_str("!");
                if (func_name) {
                    lib::kprint_str(func_name);
                } else {
                    lib::kprint_str("#");
                    lib::kprint_udec(ordinal);
                }
                lib::kprint_str(" -> ");
                lib::kprint_hex(resolved_addr);
                lib::kprint_str("\r\n");

                iat[idx] = resolved_addr;
            } else {
                lib::kprint_str("[WIN32] Warning: Unresolved import ");
                lib::kprint_str(dll_name);
                lib::kprint_str("!");
                if (func_name) {
                    lib::kprint_str(func_name);
                }
                lib::kprint_str("\r\n");
            }
            idx++;
        }
        desc++;
    }

    return true;
}

} // namespace loader

