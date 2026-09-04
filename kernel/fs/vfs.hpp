/* ==============================================================================
 * ZweiOS - Bare-Metal x86_64 Operating System
 * Component: Virtual File System (VFS) Layer
 * ============================================================================== */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

namespace fs {

// Standard file descriptor indices
inline constexpr int STDIN_FILENO  = 0;
inline constexpr int STDOUT_FILENO = 1;
inline constexpr int STDERR_FILENO = 2;

// File open flags
inline constexpr int O_RDONLY = 0x0000;
inline constexpr int O_WRONLY = 0x0001;
inline constexpr int O_RDWR   = 0x0002;
inline constexpr int O_CREAT  = 0x0040;
inline constexpr int O_TRUNC  = 0x0200;
inline constexpr int O_APPEND = 0x0400;

// Seek modes
inline constexpr int SEEK_SET = 0;
inline constexpr int SEEK_CUR = 1;
inline constexpr int SEEK_END = 2;

// Node types
enum class VNodeType {
    UNKNOWN = 0,
    FILE,
    DIRECTORY,
    DEVICE
};

// Node permissions
inline constexpr uint32_t VFS_PERM_READ  = (1U << 0);
inline constexpr uint32_t VFS_PERM_WRITE = (1U << 1);
inline constexpr uint32_t VFS_PERM_EXEC  = (1U << 2);

struct VNode;

struct VNodeOps {
    int     (*read)(VNode* node, uint64_t offset, size_t size, uint8_t* buffer);
    int     (*write)(VNode* node, uint64_t offset, size_t size, const uint8_t* buffer);
    VNode*  (*lookup)(VNode* dir, const char* name);
    int     (*create)(VNode* dir, const char* name, VNodeType type);
    int     (*remove)(VNode* dir, const char* name);
    int     (*readdir)(VNode* dir, size_t index, char* out_name, VNodeType* out_type, size_t* out_size);
};

struct VNode {
    char        name[64];
    VNodeType   type;
    uint64_t    size;
    uint32_t    permissions;
    VNodeOps*   ops;
    void*       fs_data; // Filesystem-specific private handle/pointer
};

struct VNodeStat {
    uint64_t    size;
    VNodeType   type;
    uint32_t    permissions;
};

struct FileDescriptor {
    VNode*      node;
    uint64_t    offset;
    int         flags;
    bool        is_open;
};

inline constexpr size_t MAX_OPEN_FILES = 64;

// Path processing
void vfs_normalize_path(const char* in_path, char* out_path, size_t max_len);
VNode* vfs_resolve_path(const char* path);

// Core VFS APIs
void vfs_init(void);
int vfs_open(const char* path, int flags);
int vfs_close(int fd);
int64_t vfs_read(int fd, void* buf, size_t count);
int64_t vfs_write(int fd, const void* buf, size_t count);
int64_t vfs_lseek(int fd, int64_t offset, int whence);
int vfs_stat(const char* path, VNodeStat* out_stat);
int vfs_mkdir(const char* path);
int vfs_touch(const char* path);
int vfs_remove(const char* path);

void vfs_set_root(VNode* root);
VNode* vfs_get_root(void);

} // namespace fs
