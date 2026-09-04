/* ==============================================================================
 * ZweiOS - Bare-Metal x86_64 Operating System
 * Component: System V AMD64 Initial Process Stack Builder Implementation
 * ============================================================================== */

#include "loader/stack.hpp"
#include "lib/string.hpp"

namespace loader {

// 16-byte random seed for AT_RANDOM (used by libc stack canary)
static const uint8_t default_random_seed[16] = {
    0x6a, 0x09, 0xe6, 0x67, 0xbb, 0x67, 0xae, 0x85,
    0x3c, 0x6e, 0xf3, 0x72, 0xa5, 0x4f, 0xf5, 0x3a
};

// Default environment variables if none provided
static const char* const default_envp[] = {
    "PATH=/bin:/usr/bin",
    "TERM=xterm-256color",
    "USER=zwei",
    "HOME=/",
    nullptr
};

uint64_t setup_system_v_stack(
    uint64_t stack_bottom,
    size_t stack_size,
    const BinaryInfo* info,
    int argc,
    const char* const argv[],
    const char* const envp[]
) {
    if (!info || argc <= 0 || !argv) {
        return 0;
    }

    uint64_t sp = stack_bottom + stack_size;
    const char* const* active_envp = envp ? envp : default_envp;

    // Count environment variables
    int envc = 0;
    while (active_envp[envc] != nullptr) {
        envc++;
    }

    // 1. Copy 16-byte random seed to high stack
    sp -= sizeof(default_random_seed);
    lib::memcpy(reinterpret_cast<void*>(sp), default_random_seed, sizeof(default_random_seed));
    uint64_t random_ptr = sp;

    // 2. Copy environment strings to stack
    uint64_t env_ptrs[32];
    for (int i = envc - 1; i >= 0; --i) {
        size_t len = lib::strlen(active_envp[i]) + 1;
        sp -= len;
        lib::memcpy(reinterpret_cast<void*>(sp), active_envp[i], len);
        env_ptrs[i] = sp;
    }

    // 3. Copy argument strings to stack
    uint64_t arg_ptrs[32];
    for (int i = argc - 1; i >= 0; --i) {
        size_t len = lib::strlen(argv[i]) + 1;
        sp -= len;
        lib::memcpy(reinterpret_cast<void*>(sp), argv[i], len);
        arg_ptrs[i] = sp;
    }

    // 4. Align sp down to 8-byte boundary
    sp &= ~0x7ULL;

    // 5. Build Auxiliary Vector (auxv) array
    Elf64_auxv_t auxv[16];
    size_t aux_count = 0;

    auxv[aux_count++] = { AT_PHDR,   info->phdr_vaddr ? info->phdr_vaddr : (info->image_base + 0x40) };
    auxv[aux_count++] = { AT_PHENT,  info->phentsize ? static_cast<uint64_t>(info->phentsize) : 56ULL };
    auxv[aux_count++] = { AT_PHNUM,  info->phnum ? static_cast<uint64_t>(info->phnum) : static_cast<uint64_t>(info->segment_count) };
    auxv[aux_count++] = { AT_PAGESZ, 4096 };
    auxv[aux_count++] = { AT_BASE,   0 };
    auxv[aux_count++] = { AT_FLAGS,  0 };
    auxv[aux_count++] = { AT_ENTRY,  info->entry_point };
    auxv[aux_count++] = { AT_UID,    1000 };
    auxv[aux_count++] = { AT_EUID,   1000 };
    auxv[aux_count++] = { AT_GID,    1000 };
    auxv[aux_count++] = { AT_EGID,   1000 };
    auxv[aux_count++] = { AT_SECURE, 0 };
    auxv[aux_count++] = { AT_RANDOM, random_ptr };
    auxv[aux_count++] = { AT_EXECFN, arg_ptrs[0] };
    auxv[aux_count++] = { AT_NULL,   0 };

    // 6. Calculate total words needed:
    //    argc (1) + argv pointers (argc + 1) + envp pointers (envc + 1) + auxv (aux_count * 2)
    size_t total_words = 1 + (static_cast<size_t>(argc) + 1) + (static_cast<size_t>(envc) + 1) + (aux_count * 2);
    size_t total_bytes = total_words * sizeof(uint64_t);

    // 7. Align target_sp so target_sp % 16 == 0
    uint64_t target_sp = (sp - total_bytes) & ~0xFULL;
    uint64_t* stack_words = reinterpret_cast<uint64_t*>(target_sp);
    size_t w = 0;

    // [sp + 0]: argc
    stack_words[w++] = static_cast<uint64_t>(argc);

    // [sp + 8 ..]: argv pointers
    for (int i = 0; i < argc; ++i) {
        stack_words[w++] = arg_ptrs[i];
    }
    stack_words[w++] = 0; // NULL terminator for argv

    // [sp + ...]: envp pointers
    for (int i = 0; i < envc; ++i) {
        stack_words[w++] = env_ptrs[i];
    }
    stack_words[w++] = 0; // NULL terminator for envp

    // [sp + ...]: auxv entries
    for (size_t i = 0; i < aux_count; ++i) {
        stack_words[w++] = auxv[i].a_type;
        stack_words[w++] = auxv[i].a_val;
    }

    return target_sp;
}

} // namespace loader
