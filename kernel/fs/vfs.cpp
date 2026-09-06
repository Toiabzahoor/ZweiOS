#include "fs/vfs.hpp"
#include "fs/pipe.hpp"
#include "lib/string.hpp"
#include "lib/kprintf.hpp"
#include "drivers/serial.hpp"
#include "drivers/vga.hpp"
#include "drivers/keyboard.hpp"

namespace fs {

static VNode*         g_root_vnode = nullptr;
static FileDescriptor g_fd_table[MAX_OPEN_FILES];
static MountEntry     g_mount_table[MAX_MOUNTS];
static char           g_cwd[256] = "/";

void vfs_set_root(VNode* root) {
    g_root_vnode = root;
    vfs_mount("/", 'C', root);
}

VNode* vfs_get_root(void) {
    return g_root_vnode;
}

int vfs_mount(const char* path, char drive_letter, VNode* root) {
    if (!path || !root) return -1;

    char norm[64];
    vfs_normalize_path(path, norm, sizeof(norm));

    char upper_drive = drive_letter;
    if (upper_drive >= 'a' && upper_drive <= 'z') {
        upper_drive = static_cast<char>(upper_drive - ('a' - 'A'));
    }

    for (size_t i = 0; i < MAX_MOUNTS; ++i) {
        if (g_mount_table[i].in_use) {
            if (lib::strcmp(g_mount_table[i].mount_path, norm) == 0 || (upper_drive != '\0' && g_mount_table[i].drive_letter == upper_drive)) {
                lib::strncpy(g_mount_table[i].mount_path, norm, sizeof(g_mount_table[i].mount_path));
                g_mount_table[i].drive_letter = upper_drive;
                g_mount_table[i].root_vnode = root;
                return 0;
            }
        }
    }

    for (size_t i = 0; i < MAX_MOUNTS; ++i) {
        if (!g_mount_table[i].in_use) {
            lib::strncpy(g_mount_table[i].mount_path, norm, sizeof(g_mount_table[i].mount_path));
            g_mount_table[i].drive_letter = upper_drive;
            g_mount_table[i].root_vnode = root;
            g_mount_table[i].in_use = true;
            return 0;
        }
    }

    return -1;
}

int vfs_unmount(const char* path) {
    if (!path) return -1;

    char norm[64];
    vfs_normalize_path(path, norm, sizeof(norm));

    for (size_t i = 0; i < MAX_MOUNTS; ++i) {
        if (g_mount_table[i].in_use && lib::strcmp(g_mount_table[i].mount_path, norm) == 0) {
            g_mount_table[i].in_use = false;
            g_mount_table[i].root_vnode = nullptr;
            g_mount_table[i].mount_path[0] = '\0';
            g_mount_table[i].drive_letter = '\0';
            return 0;
        }
    }

    return -1;
}

size_t vfs_get_mount_count(void) {
    size_t count = 0;
    for (size_t i = 0; i < MAX_MOUNTS; ++i) {
        if (g_mount_table[i].in_use) {
            count++;
        }
    }
    return count;
}

bool vfs_get_mount(size_t index, MountEntry* out_entry) {
    if (!out_entry) return false;

    size_t cur = 0;
    for (size_t i = 0; i < MAX_MOUNTS; ++i) {
        if (g_mount_table[i].in_use) {
            if (cur == index) {
                *out_entry = g_mount_table[i];
                return true;
            }
            cur++;
        }
    }
    return false;
}

void vfs_init(void) {
    for (size_t i = 0; i < MAX_OPEN_FILES; ++i) {
        g_fd_table[i].node = nullptr;
        g_fd_table[i].offset = 0;
        g_fd_table[i].flags = 0;
        g_fd_table[i].is_open = false;
    }

    for (size_t i = 0; i < MAX_MOUNTS; ++i) {
        g_mount_table[i].in_use = false;
        g_mount_table[i].mount_path[0] = '\0';
        g_mount_table[i].drive_letter = '\0';
        g_mount_table[i].root_vnode = nullptr;
    }

    g_fd_table[STDIN_FILENO].is_open  = true;
    g_fd_table[STDIN_FILENO].flags    = O_RDONLY;

    g_fd_table[STDOUT_FILENO].is_open = true;
    g_fd_table[STDOUT_FILENO].flags   = O_WRONLY;

    g_fd_table[STDERR_FILENO].is_open = true;
    g_fd_table[STDERR_FILENO].flags   = O_WRONLY;

    pipe_init();
    drivers::serial_puts("[VFS] Virtual File System initialized with standard streams (0, 1, 2).\r\n");
}

void vfs_normalize_path(const char* in_path, char* out_path, size_t max_len) {
    if (!in_path || !out_path || max_len == 0) {
        if (out_path && max_len > 0) out_path[0] = '\0';
        return;
    }

    char combined[256];
    const char* src = in_path;
    char drive_char = '\0';

    if ((src[0] >= 'a' && src[0] <= 'z') || (src[0] >= 'A' && src[0] <= 'Z')) {
        if (src[1] == ':') {
            drive_char = src[0];
            if (drive_char >= 'a' && drive_char <= 'z') {
                drive_char = static_cast<char>(drive_char - ('a' - 'A'));
            }
            src += 2;
        }
    }

    if (drive_char == '\0' && src[0] != '/' && src[0] != '\\') {
        size_t cwd_len = lib::strlen(g_cwd);
        lib::strncpy(combined, g_cwd, sizeof(combined) - 1);
        if (cwd_len > 0 && combined[cwd_len - 1] != '/') {
            combined[cwd_len++] = '/';
            combined[cwd_len] = '\0';
        }
        lib::strncpy(combined + cwd_len, src, sizeof(combined) - cwd_len - 1);
        src = combined;
    }

    size_t out_idx = 0;
    if (drive_char != '\0') {
        out_path[out_idx++] = drive_char;
        out_path[out_idx++] = ':';
    }

    out_path[out_idx++] = '/';

    while (*src != '\0' && out_idx < max_len - 1) {
        char c = *src++;
        if (c == '\\') {
            c = '/';
        }

        if (c == '/' && out_idx > 0 && out_path[out_idx - 1] == '/') {
            continue;
        }

        out_path[out_idx++] = c;
    }

    if (out_idx > (drive_char != '\0' ? 3 : 1) && out_path[out_idx - 1] == '/') {
        out_idx--;
    }

    out_path[out_idx] = '\0';
}

static VNode* resolve_relative_path(VNode* start_dir, const char* relative_path) {
    if (!start_dir || !relative_path) return nullptr;

    const char* p = relative_path;
    while (*p == '/') p++;

    if (*p == '\0') {
        return start_dir;
    }

    VNode* curr = start_dir;
    char token[64];

    while (*p != '\0') {
        size_t t_idx = 0;
        while (*p != '\0' && *p != '/' && *p != '\\' && t_idx < sizeof(token) - 1) {
            token[t_idx++] = *p++;
        }
        token[t_idx] = '\0';

        while (*p == '/' || *p == '\\') p++;

        if (t_idx == 0) continue;

        if (lib::strcmp(token, ".") == 0) {
            continue;
        }

        if (curr->type != VNodeType::DIRECTORY || !curr->ops || !curr->ops->lookup) {
            return nullptr;
        }

        curr = curr->ops->lookup(curr, token);
        if (!curr) {
            return nullptr;
        }
    }

    return curr;
}

VNode* vfs_resolve_path(const char* path) {
    if (!path) return nullptr;

    char norm[256];
    vfs_normalize_path(path, norm, sizeof(norm));

    if (norm[1] == ':') {
        char drive = norm[0];
        if (drive >= 'a' && drive <= 'z') drive = static_cast<char>(drive - ('a' - 'A'));

        for (size_t i = 0; i < MAX_MOUNTS; ++i) {
            if (g_mount_table[i].in_use && g_mount_table[i].drive_letter == drive) {
                return resolve_relative_path(g_mount_table[i].root_vnode, norm + 2);
            }
        }

        if (drive == 'C' && g_root_vnode) {
            return resolve_relative_path(g_root_vnode, norm + 2);
        }

        return nullptr;
    }

    size_t longest_prefix_len = 0;
    int    best_mount_idx = -1;

    for (size_t i = 0; i < MAX_MOUNTS; ++i) {
        if (!g_mount_table[i].in_use) continue;

        const char* mpath = g_mount_table[i].mount_path;
        size_t mlen = lib::strlen(mpath);

        if (mlen == 1 && mpath[0] == '/') {
            if (longest_prefix_len == 0) {
                longest_prefix_len = 1;
                best_mount_idx = static_cast<int>(i);
            }
            continue;
        }

        if (lib::strncmp(norm, mpath, mlen) == 0) {
            if (norm[mlen] == '/' || norm[mlen] == '\0') {
                if (mlen > longest_prefix_len) {
                    longest_prefix_len = mlen;
                    best_mount_idx = static_cast<int>(i);
                }
            }
        }
    }

    if (best_mount_idx != -1) {
        const char* subpath = norm + longest_prefix_len;
        if (*subpath == '\0') {
            return g_mount_table[best_mount_idx].root_vnode;
        }
        return resolve_relative_path(g_mount_table[best_mount_idx].root_vnode, subpath);
    }

    if (g_root_vnode) {
        return resolve_relative_path(g_root_vnode, norm);
    }

    return nullptr;
}

static bool split_parent_and_name(const char* norm_path, char* out_parent, size_t parent_len, char* out_name, size_t name_len) {
    if (!norm_path || !out_parent || !out_name) return false;

    size_t len = lib::strlen(norm_path);
    if (len == 0) return false;

    int last_slash = -1;
    for (int i = static_cast<int>(len) - 1; i >= 0; --i) {
        if (norm_path[i] == '/') {
            last_slash = i;
            break;
        }
    }

    if (last_slash == -1) {
        out_parent[0] = '/';
        out_parent[1] = '\0';
        lib::strncpy(out_name, norm_path, name_len);
    } else if (last_slash == 0) {
        out_parent[0] = '/';
        out_parent[1] = '\0';
        lib::strncpy(out_name, norm_path + 1, name_len);
    } else {
        size_t p_len = static_cast<size_t>(last_slash);
        if (p_len >= parent_len) p_len = parent_len - 1;
        lib::memcpy(out_parent, norm_path, p_len);
        out_parent[p_len] = '\0';
        lib::strncpy(out_name, norm_path + last_slash + 1, name_len);
    }

    return true;
}

int vfs_alloc_fd(VNode* node, int flags) {
    if (!node) return -1;
    for (int i = 3; i < static_cast<int>(MAX_OPEN_FILES); ++i) {
        if (!g_fd_table[i].is_open) {
            g_fd_table[i].node = node;
            g_fd_table[i].flags = flags;
            g_fd_table[i].offset = 0;
            g_fd_table[i].is_open = true;
            return i;
        }
    }
    return -1;
}

FileDescriptor* vfs_get_fd(int fd) {
    if (fd < 0 || fd >= static_cast<int>(MAX_OPEN_FILES) || !g_fd_table[fd].is_open) {
        return nullptr;
    }
    return &g_fd_table[fd];
}

int vfs_open(const char* path, int flags) {
    if (!path) return -1;

    char norm[256];
    vfs_normalize_path(path, norm, sizeof(norm));

    VNode* node = vfs_resolve_path(norm);

    if (!node) {
        if (flags & O_CREAT) {
            char parent_path[256];
            char file_name[64];
            if (!split_parent_and_name(norm, parent_path, sizeof(parent_path), file_name, sizeof(file_name))) {
                return -1;
            }

            VNode* parent = vfs_resolve_path(parent_path);
            if (!parent || parent->type != VNodeType::DIRECTORY || !parent->ops || !parent->ops->create) {
                return -1;
            }

            if (parent->ops->create(parent, file_name, VNodeType::FILE) != 0) {
                return -1;
            }

            node = vfs_resolve_path(norm);
            if (!node) return -1;
        } else {
            return -1;
        }
    }

    int chosen_fd = vfs_alloc_fd(node, flags);
    if (chosen_fd == -1) {
        return -1;
    }

    if (flags & O_APPEND) {
        g_fd_table[chosen_fd].offset = node->size;
    } else {
        g_fd_table[chosen_fd].offset = 0;
    }

    if ((flags & O_TRUNC) && (node->type == VNodeType::FILE)) {
        if (node->ops && node->ops->write) {
            node->ops->write(node, 0, 0, nullptr);
        }
        node->size = 0;
    }

    return chosen_fd;
}

int vfs_close(int fd) {
    if (fd < 0 || fd >= static_cast<int>(MAX_OPEN_FILES)) {
        return -1;
    }

    if (!g_fd_table[fd].is_open) {
        return -1;
    }

    if (g_fd_table[fd].node && g_fd_table[fd].node->type == VNodeType::PIPE) {
        pipe_vnode_close(g_fd_table[fd].node, (g_fd_table[fd].flags & O_WRONLY) != 0);
    }

    g_fd_table[fd].is_open = false;
    g_fd_table[fd].node = nullptr;
    g_fd_table[fd].offset = 0;
    g_fd_table[fd].flags = 0;
    return 0;
}

int64_t vfs_read(int fd, void* buf, size_t count) {
    if (fd < 0 || fd >= static_cast<int>(MAX_OPEN_FILES) || !buf) {
        return -1;
    }

    if (!g_fd_table[fd].is_open) {
        return -1;
    }

    if (g_fd_table[fd].node != nullptr) {
        VNode* node = g_fd_table[fd].node;
        if (!node->ops || !node->ops->read) {
            return -1;
        }
        int res = node->ops->read(node, g_fd_table[fd].offset, count, reinterpret_cast<uint8_t*>(buf));
        if (res > 0) {
            g_fd_table[fd].offset += static_cast<uint64_t>(res);
        }
        return res;
    }

    if (fd == STDIN_FILENO) {
        char* p = reinterpret_cast<char*>(buf);
        size_t read_bytes = 0;
        while (read_bytes < count) {
            char c = 0;
            if (drivers::sys_try_getc(&c)) {
                p[read_bytes++] = c;
                if (c == '\n' || c == '\r') break;
            } else {
                asm volatile("pause");
            }
        }
        return static_cast<int64_t>(read_bytes);
    }

    return -1;
}

int64_t vfs_write(int fd, const void* buf, size_t count) {
    if (fd < 0 || fd >= static_cast<int>(MAX_OPEN_FILES) || !buf) {
        return -1;
    }

    if (!g_fd_table[fd].is_open) {
        return -1;
    }

    if (g_fd_table[fd].node != nullptr) {
        VNode* node = g_fd_table[fd].node;
        if (!node->ops || !node->ops->write) {
            return -1;
        }
        int res = node->ops->write(node, g_fd_table[fd].offset, count, reinterpret_cast<const uint8_t*>(buf));
        if (res > 0) {
            g_fd_table[fd].offset += static_cast<uint64_t>(res);
        }
        return res;
    }

    if (fd == STDOUT_FILENO || fd == STDERR_FILENO) {
        const char* p = reinterpret_cast<const char*>(buf);
        for (size_t i = 0; i < count; ++i) {
            lib::kprint_char(p[i]);
        }
        return static_cast<int64_t>(count);
    }

    return -1;
}

int64_t vfs_lseek(int fd, int64_t offset, int whence) {
    if (fd < 0 || fd >= static_cast<int>(MAX_OPEN_FILES) || !g_fd_table[fd].is_open) {
        return -1;
    }

    VNode* node = g_fd_table[fd].node;
    if (!node) return -1;

    int64_t new_offset = 0;
    switch (whence) {
        case SEEK_SET:
            new_offset = offset;
            break;
        case SEEK_CUR:
            new_offset = static_cast<int64_t>(g_fd_table[fd].offset) + offset;
            break;
        case SEEK_END:
            new_offset = static_cast<int64_t>(node->size) + offset;
            break;
        default:
            return -1;
    }

    if (new_offset < 0) return -1;

    g_fd_table[fd].offset = static_cast<uint64_t>(new_offset);
    return new_offset;
}

int vfs_dup(int oldfd) {
    if (oldfd < 0 || oldfd >= static_cast<int>(MAX_OPEN_FILES) || !g_fd_table[oldfd].is_open) {
        return -9;
    }
    for (int i = 0; i < static_cast<int>(MAX_OPEN_FILES); ++i) {
        if (!g_fd_table[i].is_open) {
            g_fd_table[i] = g_fd_table[oldfd];
            if (g_fd_table[i].node && g_fd_table[i].node->type == VNodeType::PIPE) {
                auto* pipe = static_cast<PipeChannel*>(g_fd_table[i].node->fs_data);
                if (pipe) {
                    if (g_fd_table[i].flags & O_WRONLY) pipe->ref_write++;
                    else pipe->ref_read++;
                }
            }
            return i;
        }
    }
    return -24;
}

int vfs_dup2(int oldfd, int newfd) {
    if (oldfd < 0 || oldfd >= static_cast<int>(MAX_OPEN_FILES) || !g_fd_table[oldfd].is_open) {
        return -9;
    }
    if (newfd < 0 || newfd >= static_cast<int>(MAX_OPEN_FILES)) {
        return -9;
    }
    if (oldfd == newfd) {
        return newfd;
    }
    if (g_fd_table[newfd].is_open) {
        vfs_close(newfd);
    }
    g_fd_table[newfd] = g_fd_table[oldfd];
    if (g_fd_table[newfd].node && g_fd_table[newfd].node->type == VNodeType::PIPE) {
        auto* pipe = static_cast<PipeChannel*>(g_fd_table[newfd].node->fs_data);
        if (pipe) {
            if (g_fd_table[newfd].flags & O_WRONLY) pipe->ref_write++;
            else pipe->ref_read++;
        }
    }
    return newfd;
}

int vfs_fcntl(int fd, int cmd, uint64_t arg) {
    if (fd < 0 || fd >= static_cast<int>(MAX_OPEN_FILES) || !g_fd_table[fd].is_open) {
        return -9;
    }
    switch (cmd) {
        case F_DUPFD: {
            int start = static_cast<int>(arg);
            if (start < 0 || start >= static_cast<int>(MAX_OPEN_FILES)) return -22;
            for (int i = start; i < static_cast<int>(MAX_OPEN_FILES); ++i) {
                if (!g_fd_table[i].is_open) {
                    return vfs_dup2(fd, i);
                }
            }
            return -24;
        }
        case F_GETFD: {
            return (g_fd_table[fd].flags & O_CLOEXEC) ? FD_CLOEXEC : 0;
        }
        case F_SETFD: {
            if (arg & FD_CLOEXEC) {
                g_fd_table[fd].flags |= O_CLOEXEC;
            } else {
                g_fd_table[fd].flags &= ~O_CLOEXEC;
            }
            return 0;
        }
        case F_GETFL: {
            return g_fd_table[fd].flags;
        }
        case F_SETFL: {
            g_fd_table[fd].flags = (g_fd_table[fd].flags & ~(O_NONBLOCK | O_APPEND)) | (static_cast<int>(arg) & (O_NONBLOCK | O_APPEND));
            return 0;
        }
        default:
            return -22;
    }
}

int vfs_pipe(int pipefd[2], int flags) {
    return pipe_create(pipefd, flags);
}

int vfs_stat(const char* path, VNodeStat* out_stat) {
    if (!path || !out_stat) return -1;

    VNode* node = vfs_resolve_path(path);
    if (!node) return -1;

    out_stat->size = node->size;
    out_stat->type = node->type;
    out_stat->permissions = node->permissions;
    return 0;
}

int vfs_fstat(int fd, VNodeStat* out_stat) {
    if (fd < 0 || fd >= static_cast<int>(MAX_OPEN_FILES) || !g_fd_table[fd].is_open || !out_stat) {
        return -1;
    }
    if (g_fd_table[fd].node) {
        out_stat->size = g_fd_table[fd].node->size;
        out_stat->type = g_fd_table[fd].node->type;
        out_stat->permissions = g_fd_table[fd].node->permissions;
    } else {
        out_stat->size = 0;
        out_stat->type = VNodeType::DEVICE;
        out_stat->permissions = 0666;
    }
    return 0;
}

int vfs_access(const char* path, int mode) {
    (void)mode;
    if (!path) return -1;
    VNode* node = vfs_resolve_path(path);
    return node ? 0 : -1;
}

int vfs_mkdir(const char* path) {
    if (!path) return -1;

    char norm[256];
    vfs_normalize_path(path, norm, sizeof(norm));

    char parent_path[256];
    char dir_name[64];
    if (!split_parent_and_name(norm, parent_path, sizeof(parent_path), dir_name, sizeof(dir_name))) {
        return -1;
    }

    VNode* parent = vfs_resolve_path(parent_path);
    if (!parent || parent->type != VNodeType::DIRECTORY || !parent->ops || !parent->ops->create) {
        return -1;
    }

    return parent->ops->create(parent, dir_name, VNodeType::DIRECTORY);
}

int vfs_touch(const char* path) {
    int fd = vfs_open(path, O_CREAT | O_WRONLY);
    if (fd < 0) return -1;
    vfs_close(fd);
    return 0;
}

int vfs_remove(const char* path) {
    if (!path) return -1;

    char norm[256];
    vfs_normalize_path(path, norm, sizeof(norm));

    char parent_path[256];
    char name[64];
    if (!split_parent_and_name(norm, parent_path, sizeof(parent_path), name, sizeof(name))) {
        return -1;
    }

    VNode* parent = vfs_resolve_path(parent_path);
    if (!parent || parent->type != VNodeType::DIRECTORY || !parent->ops || !parent->ops->remove) {
        return -1;
    }

    return parent->ops->remove(parent, name);
}

int vfs_unlink(const char* path) {
    return vfs_remove(path);
}

int vfs_rename(const char* oldpath, const char* newpath) {
    if (!oldpath || !newpath) return -1;
    VNode* node = vfs_resolve_path(oldpath);
    if (!node || node->type != VNodeType::FILE) return -1;

    int rfd = vfs_open(oldpath, O_RDONLY);
    if (rfd < 0) return -1;

    int wfd = vfs_open(newpath, O_CREAT | O_WRONLY | O_TRUNC);
    if (wfd < 0) {
        vfs_close(rfd);
        return -1;
    }

    uint8_t buf[512];
    int64_t bytes = 0;
    while ((bytes = vfs_read(rfd, buf, sizeof(buf))) > 0) {
        vfs_write(wfd, buf, static_cast<size_t>(bytes));
    }

    vfs_close(rfd);
    vfs_close(wfd);
    vfs_remove(oldpath);
    return 0;
}

int vfs_getcwd(char* buf, size_t size) {
    if (!buf || size == 0) return -1;
    lib::strncpy(buf, g_cwd, size);
    return 0;
}

int vfs_chdir(const char* path) {
    if (!path) return -1;
    char norm[256];
    vfs_normalize_path(path, norm, sizeof(norm));
    VNode* node = vfs_resolve_path(norm);
    if (!node || node->type != VNodeType::DIRECTORY) return -1;
    lib::strncpy(g_cwd, norm, sizeof(g_cwd));
    return 0;
}

}
