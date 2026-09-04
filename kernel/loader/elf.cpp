/* ==============================================================================
 * ZweiOS - Bare-Metal x86_64 Operating System
 * Component: 64-bit ELF (Executable and Linkable Format) Implementation
 * ============================================================================== */

#include "loader/elf.hpp"
#include "lib/string.hpp"

namespace loader {

bool elf_probe(const uint8_t* data, size_t size) {
    if (!data || size < sizeof(Elf64_Ehdr)) {
        return false;
    }

    const auto* ehdr = reinterpret_cast<const Elf64_Ehdr*>(data);

    if (ehdr->e_ident[0] != ELFMAG0 ||
        ehdr->e_ident[1] != ELFMAG1 ||
        ehdr->e_ident[2] != ELFMAG2 ||
        ehdr->e_ident[3] != ELFMAG3) {
        return false;
    }

    if (ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
        return false;
    }

    if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB) {
        return false;
    }

    if (ehdr->e_machine != EM_X86_64) {
        return false;
    }

    if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) {
        return false;
    }

    return true;
}

bool elf_parse(const uint8_t* data, size_t size, BinaryInfo* out_info) {
    if (!elf_probe(data, size) || !out_info) {
        return false;
    }

    const auto* ehdr = reinterpret_cast<const Elf64_Ehdr*>(data);

    // Verify program header table falls within file boundaries
    uint64_t ph_end = ehdr->e_phoff + (static_cast<uint64_t>(ehdr->e_phnum) * sizeof(Elf64_Phdr));
    if (ph_end > size || ehdr->e_phentsize < sizeof(Elf64_Phdr)) {
        return false;
    }

    lib::memset(out_info, 0, sizeof(BinaryInfo));
    out_info->format = BinaryFormat::ELF64;
    out_info->arch = BinaryArch::X86_64;
    out_info->subsystem = BinarySubsystem::LINUX_CLI;
    out_info->entry_point = ehdr->e_entry;
    out_info->phnum = ehdr->e_phnum;
    out_info->phentsize = ehdr->e_phentsize;
    out_info->phdr_vaddr = 0;

    uint64_t min_vaddr = UINT64_MAX;
    uint64_t max_vaddr = 0;
    size_t seg_idx = 0;

    const uint8_t* ph_base = data + ehdr->e_phoff;

    for (uint16_t i = 0; i < ehdr->e_phnum; ++i) {
        const auto* phdr = reinterpret_cast<const Elf64_Phdr*>(ph_base + (i * ehdr->e_phentsize));

        if (phdr->p_type == PT_PHDR) {
            out_info->phdr_vaddr = phdr->p_vaddr;
            continue;
        }

        if (phdr->p_type != PT_LOAD) {
            continue;
        }

        if (seg_idx >= MAX_BINARY_SEGMENTS) {
            break;
        }

        // Validate segment offset and size inside file
        if (phdr->p_offset + phdr->p_filesz > size) {
            return false;
        }

        BinarySegment* seg = &out_info->segments[seg_idx];
        
        // Construct segment name: "LOAD0", "LOAD1", etc.
        seg->name[0] = 'L'; seg->name[1] = 'O'; seg->name[2] = 'A'; seg->name[3] = 'D';
        seg->name[4] = static_cast<char>('0' + (seg_idx % 10));
        seg->name[5] = '\0';

        seg->vaddr = phdr->p_vaddr;
        seg->mem_size = phdr->p_memsz;
        seg->file_offset = phdr->p_offset;
        seg->file_size = phdr->p_filesz;
        seg->alignment = phdr->p_align;
        seg->flags = 0;

        if (phdr->p_flags & PF_R) seg->flags |= SEG_FLAG_READ;
        if (phdr->p_flags & PF_W) seg->flags |= SEG_FLAG_WRITE;
        if (phdr->p_flags & PF_X) seg->flags |= SEG_FLAG_EXEC;

        if (phdr->p_vaddr < min_vaddr) {
            min_vaddr = phdr->p_vaddr;
        }
        if (phdr->p_vaddr + phdr->p_memsz > max_vaddr) {
            max_vaddr = phdr->p_vaddr + phdr->p_memsz;
        }

        seg_idx++;
    }

    out_info->segment_count = seg_idx;
    if (min_vaddr != UINT64_MAX && max_vaddr >= min_vaddr) {
        out_info->image_base = min_vaddr;
        out_info->image_size = max_vaddr - min_vaddr;
    }

    if (out_info->phdr_vaddr == 0 && out_info->segment_count > 0) {
        for (size_t s = 0; s < out_info->segment_count; ++s) {
            if (out_info->segments[s].file_offset == 0) {
                out_info->phdr_vaddr = out_info->segments[s].vaddr + ehdr->e_phoff;
                break;
            }
        }
    }

    return true;
}

} // namespace loader
