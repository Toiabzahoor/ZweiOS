/* ==============================================================================
 * ZweiOS - Bare-Metal x86_64 Operating System
 * Component: Virtual File System (VFS) Implementation
 * ============================================================================== */

#include "fs/vfs.hpp"
#include "lib/string.hpp"
#include "lib/kprintf.hpp"
#include "drivers/serial.hpp"
#include "drivers/vga.hpp"
#include "drivers/keyboard.hpp"

namespace fs {

static VNode* g_root_vnode = nullptr;
static FileDescriptor g_fd_table[MAX_OPEN_FILES];

void vfs_set_root(VNode* root) {
    g_root_vnode = root;
}

VNode* vfs_get_root(void) {
    return g_root_vnode;
}

void vfs_init(void) {
    for (size_t i = 0; i < MAX_OPEN_FILES; ++i) {
        g_fd_table[i].node = nullptr;
        g_fd_table[i].offset = 0;
        g_fd_table[i].flags = 0;
        g_fd_table[i].is_open = false;
    }

    // Standard I/O Descriptors
    g_fd_table[STDIN_FILENO].is_open  = true;
    g_fd_table[STDIN_FILENO].flags    = O_RDONLY;

    g_fd_table[STDOUT_FILENO].is_open = true;
    g_fd_table[STDOUT_FILENO].flags   = O_WRONLY;

    g_fd_table[STDERR_FILENO].is_open = true;
    g_fd_table[STDERR_FILENO].flags   = O_WRONLY;

    drivers::serial_puts("[VFS] Virtual File System initialized with standard streams (0, 1, 2).\r\n");
}

void vfs_normalize_path(const char* in_path, char* out_path, size_t max_len) {
    if (!in_path || !out_path || max_len == 0) {
        if (out_path && max_len > 0) out_path[0] = '\0';
        return;
    }

    const char* src = in_path;

    // Skip Windows drive specifier (e.g. "C:" or "c:")
    if ((src[0] >= 'a' && src[0] <= 'z') || (src[0] >= 'A' && src[0] <= 'Z')) {
        if (src[1] == ':') {
            src += 2;
        }
    }

    size_t out_idx = 0;

    // Ensure leading slash
    out_path[out_idx++] = '/';

    while (*src != '\0' && out_idx < max_len - 1) {
        char c = *src++;
        if (c == '\\') {
            c = '/';
        }

        // Collapse duplicate slashes
        if (c == '/' && out_idx > 0 && out_path[out_idx - 1] == '/') {
            continue;
        }

        out_path[out_idx++] = c;
    }

    // Strip trailing slash if path is longer than root "/"
    if (out_idx > 1 && out_path[out_idx - 1] == '/') {
        out_idx--;
    }

    out_path[out_idx] = '\0';
}

VNode* vfs_resolve_path(const char* path) {
    if (!path || !g_root_vnode) {
        return nullptr;
    }

    char norm[256];
    vfs_normalize_path(path, norm, sizeof(norm));

    if (lib::strcmp(norm, "/") == 0) {
        return g_root_vnode;
    }

    VNode* curr = g_root_vnode;
    char token[64];
    const char* p = norm;
    if (*p == '/') p++;

    while (*p != '\0') {
        size_t t_idx = 0;
        while (*p != '\0' && *p != '/' && t_idx < sizeof(token) - 1) {
            token[t_idx++] = *p++;
        }
        token[t_idx] = '\0';

        if (*p == '/') p++;

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

static bool split_parent_and_name(const char* norm_path, char* out_parent, size_t parent_len, char* out_name, size_t name_len) {
    size_t len = lib::strlen(norm_path);
    if (len == 0 || (len == 1 && norm_path[0] == '/')) {
        return false;
    }

    int last_slash = -1;
    for (int i = static_cast<int>(len) - 1; i >= 0; --i) {
        if (norm_path[i] == '/') {
            last_slash = i;
            break;
        }
    }

    if (last_slash <= 0) {
        // Parent is root "/"
        lib::strncpy(out_parent, "/", parent_len);
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

    // Allocate free file descriptor slot
    int chosen_fd = -1;
    for (int i = 3; i < static_cast<int>(MAX_OPEN_FILES); ++i) {
        if (!g_fd_table[i].is_open) {
            chosen_fd = i;
            break;
        }
    }

    if (chosen_fd == -1) {
        return -1; // Exhausted descriptors
    }

    g_fd_table[chosen_fd].node = node;
    g_fd_table[chosen_fd].flags = flags;
    g_fd_table[chosen_fd].is_open = true;

    if (flags & O_APPEND) {
        g_fd_table[chosen_fd].offset = node->size;
    } else {
        g_fd_table[chosen_fd].offset = 0;
    }

    if ((flags & O_TRUNC) && (node->type == VNodeType::FILE)) {
        if (node->ops && node->ops->write) {
            node->ops->write(node, 0, 0, nullptr); // Truncate
        }
        node->size = 0;
    }

    return chosen_fd;
}

int vfs_close(int fd) {
    if (fd < 0 || fd >= static_cast<int>(MAX_OPEN_FILES)) {
        return -1;
    }

    // Standard descriptors 0, 1, 2 remain open
    if (fd < 3) {
        return 0;
    }

    if (!g_fd_table[fd].is_open) {
        return -1;
    }

    g_fd_table[fd].is_open = false;
    g_fd_table[fd].node = nullptr;
    g_fd_table[fd].offset = 0;
    return 0;
}

int64_t vfs_read(int fd, void* buf, size_t count) {
    if (fd < 0 || fd >= static_cast<int>(MAX_OPEN_FILES) || !buf) {
        return -1;
    }

    if (!g_fd_table[fd].is_open) {
        return -1;
    }

    // Standard input (keyboard)
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

    VNode* node = g_fd_table[fd].node;
    if (!node || !node->ops || !node->ops->read) {
        return -1;
    }

    int res = node->ops->read(node, g_fd_table[fd].offset, count, reinterpret_cast<uint8_t*>(buf));
    if (res > 0) {
        g_fd_table[fd].offset += static_cast<uint64_t>(res);
    }
    return res;
}

int64_t vfs_write(int fd, const void* buf, size_t count) {
    if (fd < 0 || fd >= static_cast<int>(MAX_OPEN_FILES) || !buf) {
        return -1;
    }

    if (!g_fd_table[fd].is_open) {
        return -1;
    }

    // Standard output & Standard error
    if (fd == STDOUT_FILENO || fd == STDERR_FILENO) {
        const char* p = reinterpret_cast<const char*>(buf);
        for (size_t i = 0; i < count; ++i) {
            drivers::serial_putc(p[i]);
            drivers::vga_putc(p[i]);
        }
        return static_cast<int64_t>(count);
    }

    VNode* node = g_fd_table[fd].node;
    if (!node || !node->ops || !node->ops->write) {
        return -1;
    }

    int res = node->ops->write(node, g_fd_table[fd].offset, count, reinterpret_cast<const uint8_t*>(buf));
    if (res > 0) {
        g_fd_table[fd].offset += static_cast<uint64_t>(res);
    }
    return res;
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

int vfs_stat(const char* path, VNodeStat* out_stat) {
    if (!path || !out_stat) return -1;

    VNode* node = vfs_resolve_path(path);
    if (!node) return -1;

    out_stat->size = node->size;
    out_stat->type = node->type;
    out_stat->permissions = node->permissions;
    return 0;
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

} // namespace fs
