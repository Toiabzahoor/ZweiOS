#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "fs/vfs.hpp"

namespace fs {

inline constexpr size_t MAX_PIPES = 16;
inline constexpr size_t PIPE_CAPACITY = 4096;

struct PipeChannel {
    uint8_t  buffer[PIPE_CAPACITY];
    size_t   head;
    size_t   tail;
    size_t   count;
    bool     read_open;
    bool     write_open;
    int      ref_read;
    int      ref_write;
    bool     in_use;
    VNode    read_vnode;
    VNode    write_vnode;
};

void pipe_init();
int pipe_create(int pipefd[2], int flags);
int pipe_vnode_read(VNode* node, uint64_t offset, size_t size, uint8_t* buffer);
int pipe_vnode_write(VNode* node, uint64_t offset, size_t size, const uint8_t* buffer);
void pipe_vnode_close(VNode* node, bool is_write);

}
