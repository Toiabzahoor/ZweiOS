/* ==============================================================================
 * ZweiOS - Bare-Metal x86_64 Operating System
 * Component: In-Memory Root RAMFS (Hierarchical In-Memory Filesystem)
 * ============================================================================== */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "fs/vfs.hpp"

namespace fs {

inline constexpr size_t RAMFS_MAX_CHILDREN = 32;

struct RamfsFile {
    uint8_t* data;
    size_t   capacity;
    size_t   size;
};

struct RamfsDir {
    VNode*   children[RAMFS_MAX_CHILDREN];
    size_t   child_count;
};

void   ramfs_init(void);
VNode* ramfs_create_dir(VNode* parent, const char* name);
VNode* ramfs_create_file(VNode* parent, const char* name, const uint8_t* initial_data, size_t size);

} // namespace fs
