

#pragma once

#include <stdint.h>
#include <stddef.h>

namespace mm {

inline constexpr size_t PAGE_SIZE = 4096;
inline constexpr size_t DEFAULT_RAM_MB = 128;
inline constexpr size_t TOTAL_FRAMES = (DEFAULT_RAM_MB * 1024 * 1024) / PAGE_SIZE;
inline constexpr size_t BITMAP_WORDS = TOTAL_FRAMES / 64;

void pmm_init(void* boot_info = nullptr);
uint64_t pmm_alloc_frame();
uint64_t pmm_alloc_contiguous_frames(size_t count);
void pmm_free_frame(uint64_t phys_addr);
void pmm_get_stats(size_t* total_frames, size_t* used_frames, size_t* free_frames);

}

using mm::PAGE_SIZE;
using mm::pmm_init;
using mm::pmm_alloc_frame;
using mm::pmm_alloc_contiguous_frames;
using mm::pmm_free_frame;
using mm::pmm_get_stats;
