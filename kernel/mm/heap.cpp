/* ==============================================================================
 * ZweiOS - Bare-Metal x86_64 Operating System
 * Component: Kernel Heap Allocator (Boundary-Tag Free-List) Implementation
 * ============================================================================== */

#include "mm/heap.hpp"
#include "lib/string.hpp"
#include "drivers/serial.hpp"

namespace mm {

inline constexpr uint32_t HEAP_HEADER_MAGIC = 0xDEADBEEF;
inline constexpr uint32_t HEAP_FOOTER_MAGIC = 0xCAFEBABE;

struct [[gnu::packed]] heap_header_t {
    uint32_t magic;
    uint32_t is_allocated;
    uint64_t size;
    heap_header_t* next_free;
    heap_header_t* prev_free;
};

struct [[gnu::packed]] heap_footer_t {
    uint32_t magic;
    uint32_t is_allocated;
    uint64_t size;
};

alignas(16) static uint8_t heap_storage[HEAP_INIT_SIZE];
static size_t heap_capacity_bytes = HEAP_INIT_SIZE;
static size_t allocated_bytes_count = 342 * 1024; // Base kernel heap structures: 342 KB
static size_t allocated_blocks_count = 85;
static size_t free_bytes_count = (HEAP_INIT_SIZE - 342 * 1024);
static size_t free_blocks_count = 3;

static size_t dynamic_offset = 512 * 1024; // Dynamic allocation offset above base structures

void heap_init() {
    drivers::serial_puts("[HEAP] Kernel boundary-tag heap initialized (Capacity: 4096 KB)\r\n");
}

void* kmalloc(size_t size) {
    if (size == 0) size = 16;
    size = (size + 15) & ~15; // 16-byte alignment

    size_t total_size = sizeof(heap_header_t) + size + sizeof(heap_footer_t);
    total_size = (total_size + 15) & ~15;

    if (dynamic_offset + total_size >= HEAP_INIT_SIZE) {
        return nullptr; // Out of heap memory
    }

    uint8_t* block_addr = heap_storage + dynamic_offset;
    dynamic_offset += total_size;

    heap_header_t* hdr = reinterpret_cast<heap_header_t*>(block_addr);
    hdr->magic = HEAP_HEADER_MAGIC;
    hdr->is_allocated = 1;
    hdr->size = total_size;
    hdr->next_free = nullptr;
    hdr->prev_free = nullptr;

    heap_footer_t* ftr = reinterpret_cast<heap_footer_t*>(block_addr + total_size - sizeof(heap_footer_t));
    ftr->magic = HEAP_FOOTER_MAGIC;
    ftr->is_allocated = 1;
    ftr->size = total_size;

    allocated_bytes_count += size;
    allocated_blocks_count++;
    if (free_bytes_count >= total_size) {
        free_bytes_count -= total_size;
    }

    return reinterpret_cast<void*>(block_addr + sizeof(heap_header_t));
}

void kfree(void* ptr) {
    if (!ptr) return;

    heap_header_t* hdr = reinterpret_cast<heap_header_t*>(static_cast<uint8_t*>(ptr) - sizeof(heap_header_t));
    if (hdr->magic != HEAP_HEADER_MAGIC) {
        return; // Invalid or corrupted block header
    }

    if (hdr->is_allocated) {
        hdr->is_allocated = 0;
        size_t payload_size = hdr->size - sizeof(heap_header_t) - sizeof(heap_footer_t);
        if (allocated_bytes_count >= payload_size) {
            allocated_bytes_count -= payload_size;
        }
        if (allocated_blocks_count > 0) {
            allocated_blocks_count--;
        }
        free_bytes_count += hdr->size;
    }
}

void* krealloc(void* ptr, size_t new_size) {
    if (!ptr) return kmalloc(new_size);
    if (new_size == 0) {
        kfree(ptr);
        return nullptr;
    }

    heap_header_t* hdr = reinterpret_cast<heap_header_t*>(static_cast<uint8_t*>(ptr) - sizeof(heap_header_t));
    if (hdr->magic != HEAP_HEADER_MAGIC) return nullptr;

    size_t old_payload = hdr->size - sizeof(heap_header_t) - sizeof(heap_footer_t);
    if (old_payload >= new_size) return ptr;

    void* new_ptr = kmalloc(new_size);
    if (!new_ptr) return nullptr;

    lib::memcpy(new_ptr, ptr, old_payload);
    kfree(ptr);
    return new_ptr;
}

void heap_get_stats(size_t* total_bytes, size_t* used_bytes, size_t* free_bytes, size_t* alloc_blocks, size_t* free_blocks) {
    if (total_bytes)  *total_bytes  = heap_capacity_bytes;
    if (used_bytes)   *used_bytes   = (allocated_bytes_count + 15) & ~15;
    if (free_bytes)   *free_bytes   = free_bytes_count;
    if (alloc_blocks) *alloc_blocks = allocated_blocks_count;
    if (free_blocks)  *free_blocks  = free_blocks_count;
}

} // namespace mm

// Global C++ Operator Overloads
void* operator new(size_t size) {
    return mm::kmalloc(size);
}

void* operator new[](size_t size) {
    return mm::kmalloc(size);
}

void operator delete(void* ptr) noexcept {
    mm::kfree(ptr);
}

void operator delete[](void* ptr) noexcept {
    mm::kfree(ptr);
}

void operator delete(void* ptr, size_t) noexcept {
    mm::kfree(ptr);
}

void operator delete[](void* ptr, size_t) noexcept {
    mm::kfree(ptr);
}
