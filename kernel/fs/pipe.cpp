#include "fs/pipe.hpp"
#include "fs/vfs.hpp"
#include "proc/sched.hpp"
#include "lib/string.hpp"

namespace fs {

static PipeChannel g_pipes[MAX_PIPES];
static VNodeOps    g_pipe_read_ops;
static VNodeOps    g_pipe_write_ops;
static bool        g_pipe_ops_inited = false;

int pipe_vnode_read(VNode* node, uint64_t offset, size_t size, uint8_t* buffer) {
    (void)offset;
    if (!node || !buffer) return -1;
    auto* pipe = static_cast<PipeChannel*>(node->fs_data);
    if (!pipe || !pipe->in_use) return -1;

    if (pipe->count == 0) {
        if (!pipe->write_open) {
            return 0;
        }
        return -11;
    }

    size_t to_read = (size < pipe->count) ? size : pipe->count;
    for (size_t i = 0; i < to_read; ++i) {
        buffer[i] = pipe->buffer[pipe->tail];
        pipe->tail = (pipe->tail + 1) % PIPE_CAPACITY;
    }
    pipe->count -= to_read;
    return static_cast<int>(to_read);
}

int pipe_vnode_write(VNode* node, uint64_t offset, size_t size, const uint8_t* buffer) {
    (void)offset;
    if (!node || !buffer) return -1;
    auto* pipe = static_cast<PipeChannel*>(node->fs_data);
    if (!pipe || !pipe->in_use) return -1;

    if (!pipe->read_open) {
        return -32;
    }

    if (size == 0) return 0;

    size_t written = 0;
    while (written < size) {
        if (pipe->count >= PIPE_CAPACITY) {
            if (!pipe->read_open) return -32;
            break;
        }
        pipe->buffer[pipe->head] = buffer[written++];
        pipe->head = (pipe->head + 1) % PIPE_CAPACITY;
        pipe->count++;
    }

    return static_cast<int>(written);
}

void pipe_vnode_close(VNode* node, bool is_write) {
    if (!node) return;
    auto* pipe = static_cast<PipeChannel*>(node->fs_data);
    if (!pipe || !pipe->in_use) return;

    if (is_write) {
        if (pipe->ref_write > 0) {
            pipe->ref_write--;
        }
        if (pipe->ref_write == 0) {
            pipe->write_open = false;
        }
    } else {
        if (pipe->ref_read > 0) {
            pipe->ref_read--;
        }
        if (pipe->ref_read == 0) {
            pipe->read_open = false;
        }
    }

    if (pipe->ref_read == 0 && pipe->ref_write == 0) {
        pipe->in_use = false;
        pipe->count = 0;
        pipe->head = 0;
        pipe->tail = 0;
    }
}

void pipe_init() {
    if (!g_pipe_ops_inited) {
        g_pipe_read_ops.read = pipe_vnode_read;
        g_pipe_read_ops.write = nullptr;
        g_pipe_read_ops.lookup = nullptr;
        g_pipe_read_ops.create = nullptr;
        g_pipe_read_ops.remove = nullptr;
        g_pipe_read_ops.readdir = nullptr;

        g_pipe_write_ops.read = nullptr;
        g_pipe_write_ops.write = pipe_vnode_write;
        g_pipe_write_ops.lookup = nullptr;
        g_pipe_write_ops.create = nullptr;
        g_pipe_write_ops.remove = nullptr;
        g_pipe_write_ops.readdir = nullptr;

        for (size_t i = 0; i < MAX_PIPES; ++i) {
            g_pipes[i].head = 0;
            g_pipes[i].tail = 0;
            g_pipes[i].count = 0;
            g_pipes[i].read_open = false;
            g_pipes[i].write_open = false;
            g_pipes[i].ref_read = 0;
            g_pipes[i].ref_write = 0;
            g_pipes[i].in_use = false;
        }

        g_pipe_ops_inited = true;
    }
}

int pipe_create(int pipefd[2], int flags) {
    if (!pipefd) return -1;
    pipe_init();

    size_t chosen = MAX_PIPES;
    for (size_t i = 0; i < MAX_PIPES; ++i) {
        if (!g_pipes[i].in_use) {
            chosen = i;
            break;
        }
    }

    if (chosen == MAX_PIPES) {
        return -24;
    }

    PipeChannel* pipe = &g_pipes[chosen];
    pipe->head = 0;
    pipe->tail = 0;
    pipe->count = 0;
    pipe->read_open = true;
    pipe->write_open = true;
    pipe->ref_read = 1;
    pipe->ref_write = 1;
    pipe->in_use = true;

    lib::strncpy(pipe->read_vnode.name, "pipe:[r]", sizeof(pipe->read_vnode.name));
    pipe->read_vnode.type = VNodeType::PIPE;
    pipe->read_vnode.size = 0;
    pipe->read_vnode.permissions = VFS_PERM_READ;
    pipe->read_vnode.ops = &g_pipe_read_ops;
    pipe->read_vnode.fs_data = pipe;

    lib::strncpy(pipe->write_vnode.name, "pipe:[w]", sizeof(pipe->write_vnode.name));
    pipe->write_vnode.type = VNodeType::PIPE;
    pipe->write_vnode.size = 0;
    pipe->write_vnode.permissions = VFS_PERM_WRITE;
    pipe->write_vnode.ops = &g_pipe_write_ops;
    pipe->write_vnode.fs_data = pipe;

    int rfd = vfs_alloc_fd(&pipe->read_vnode, O_RDONLY | flags);
    if (rfd < 0) {
        pipe->in_use = false;
        return -24;
    }

    int wfd = vfs_alloc_fd(&pipe->write_vnode, O_WRONLY | flags);
    if (wfd < 0) {
        vfs_close(rfd);
        pipe->in_use = false;
        return -24;
    }

    pipefd[0] = rfd;
    pipefd[1] = wfd;
    return 0;
}

}
