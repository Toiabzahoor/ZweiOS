

#pragma once

#include <stdint.h>
#include <stddef.h>

namespace mm {

inline constexpr uint64_t HEAP_START_ADDR = 0xFFFFFFFF90000000ULL;
inline constexpr size_t   HEAP_INIT_SIZE  = 64 * 1024 * 1024;

void heap_init();
void* kmalloc(size_t size);
void kfree(void* ptr);
void* krealloc(void* ptr, size_t new_size);
void heap_get_stats(size_t* total_bytes, size_t* used_bytes, size_t* free_bytes, size_t* alloc_blocks, size_t* free_blocks);

}

using mm::kmalloc;
using mm::kfree;
using mm::krealloc;
using mm::heap_init;
using mm::heap_get_stats;


void* operator new(size_t size);
void* operator new[](size_t size);
void operator delete(void* ptr) noexcept;
void operator delete[](void* ptr) noexcept;
void operator delete(void* ptr, size_t) noexcept;
void operator delete[](void* ptr, size_t) noexcept;
inline void* operator new(size_t, void* ptr) noexcept { return ptr; }
inline void* operator new[](size_t, void* ptr) noexcept { return ptr; }
