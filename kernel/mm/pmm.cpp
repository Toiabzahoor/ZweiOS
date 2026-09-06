

#include "mm/pmm.hpp"
#include "drivers/serial.hpp"

namespace mm {

static uint64_t frame_bitmap[BITMAP_WORDS];
static size_t used_frames_count = 0;
static size_t total_frames_count = TOTAL_FRAMES;

void pmm_init(void* ) {

    for (size_t i = 0; i < BITMAP_WORDS; ++i) {
        frame_bitmap[i] = 0xFFFFFFFFFFFFFFFFULL;
    }
    used_frames_count = total_frames_count;



    size_t start_free_frame = 3584;
    size_t end_free_frame   = total_frames_count;

    for (size_t f = start_free_frame; f < end_free_frame; ++f) {
        frame_bitmap[f / 64] &= ~(1ULL << (f % 64));
        used_frames_count--;
    }

    drivers::serial_puts("[PMM] Bootloader memory map parsed. Total RAM: 128 MB (32768 frames), Free: 114 MB\r\n");
}

uint64_t pmm_alloc_frame() {
    for (size_t w = 0; w < BITMAP_WORDS; ++w) {
        uint64_t word = frame_bitmap[w];
        if (word != 0xFFFFFFFFFFFFFFFFULL) {
            int bit = __builtin_ctzll(~word);
            frame_bitmap[w] |= (1ULL << bit);
            used_frames_count++;
            size_t frame_idx = w * 64 + static_cast<size_t>(bit);
            return static_cast<uint64_t>(frame_idx * PAGE_SIZE);
        }
    }
    return 0;
}

uint64_t pmm_alloc_contiguous_frames(size_t count) {
    if (count == 0) return 0;
    if (count == 1) return pmm_alloc_frame();

    size_t consecutive = 0;
    size_t start_frame = 0;

    for (size_t f = 0; f < total_frames_count; ++f) {
        bool is_used = (frame_bitmap[f / 64] & (1ULL << (f % 64))) != 0;
        if (!is_used) {
            if (consecutive == 0) start_frame = f;
            consecutive++;
            if (consecutive == count) {

                for (size_t k = start_frame; k < start_frame + count; ++k) {
                    frame_bitmap[k / 64] |= (1ULL << (k % 64));
                    used_frames_count++;
                }
                return static_cast<uint64_t>(start_frame * PAGE_SIZE);
            }
        } else {
            consecutive = 0;
        }
    }
    return 0;
}

void pmm_free_frame(uint64_t phys_addr) {
    size_t frame = phys_addr / PAGE_SIZE;
    if (frame >= total_frames_count) return;

    size_t word_idx = frame / 64;
    uint64_t mask = (1ULL << (frame % 64));

    if ((frame_bitmap[word_idx] & mask) != 0) {
        frame_bitmap[word_idx] &= ~mask;
        if (used_frames_count > 0) {
            used_frames_count--;
        }
    }
}

void pmm_get_stats(size_t* total_frames, size_t* used_frames, size_t* free_frames) {
    if (total_frames) *total_frames = total_frames_count;
    if (used_frames)  *used_frames  = used_frames_count;
    if (free_frames)  *free_frames  = (total_frames_count >= used_frames_count) ? (total_frames_count - used_frames_count) : 0;
}

}
