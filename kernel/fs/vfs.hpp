#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

namespace fs {

inline constexpr int STDIN_FILENO  = 0;
inline constexpr int STDOUT_FILENO = 1;
inline constexpr int STDERR_FILENO = 2;

inline constexpr int O_RDONLY   = 0x0000;
inline constexpr int O_WRONLY   = 0x0001;
inline constexpr int O_RDWR     = 0x0002;
inline constexpr int O_CREAT    = 0x0040;
inline constexpr int O_TRUNC    = 0x0200;
inline constexpr int O_APPEND   = 0x0400;
inline constexpr int O_NONBLOCK = 0x0800;
inline constexpr int O_CLOEXEC  = 0x80000;

inline constexpr int FD_CLOEXEC = 1;

inline constexpr int F_DUPFD = 0;
inline constexpr int F_GETFD = 1;
inline constexpr int F_SETFD = 2;
inline constexpr int F_GETFL = 3;
inline constexpr int F_SETFL = 4;

inline constexpr int SEEK_SET = 0;
inline constexpr int SEEK_CUR = 1;
inline constexpr int SEEK_END = 2;

enum class VNodeType {
    UNKNOWN = 0,
    FILE,
    DIRECTORY,
    DEVICE,
    PIPE
};

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
    void*       fs_data;
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

struct MountEntry {
    char   mount_path[64];
    char   drive_letter;
    VNode* root_vnode;
    bool   in_use;
};

inline constexpr size_t MAX_OPEN_FILES = 64;
inline constexpr size_t MAX_MOUNTS     = 8;

void vfs_init(void);
void vfs_normalize_path(const char* in_path, char* out_path, size_t max_len);
VNode* vfs_resolve_path(const char* path);

int vfs_mount(const char* path, char drive_letter, VNode* root);
int vfs_unmount(const char* path);
size_t vfs_get_mount_count(void);
bool vfs_get_mount(size_t index, MountEntry* out_entry);

int vfs_alloc_fd(VNode* node, int flags);
FileDescriptor* vfs_get_fd(int fd);

int vfs_open(const char* path, int flags);
int vfs_close(int fd);
int64_t vfs_read(int fd, void* buf, size_t count);
int64_t vfs_write(int fd, const void* buf, size_t count);
int64_t vfs_lseek(int fd, int64_t offset, int whence);

int vfs_dup(int oldfd);
int vfs_dup2(int oldfd, int newfd);
int vfs_fcntl(int fd, int cmd, uint64_t arg);
int vfs_pipe(int pipefd[2], int flags);

int vfs_stat(const char* path, VNodeStat* out_stat);
int vfs_fstat(int fd, VNodeStat* out_stat);
int vfs_access(const char* path, int mode);
int vfs_unlink(const char* path);
int vfs_rename(const char* oldpath, const char* newpath);
int vfs_mkdir(const char* path);
int vfs_touch(const char* path);
int vfs_remove(const char* path);
int vfs_getcwd(char* buf, size_t size);
int vfs_chdir(const char* path);

void vfs_set_root(VNode* root);
VNode* vfs_get_root(void);

}
