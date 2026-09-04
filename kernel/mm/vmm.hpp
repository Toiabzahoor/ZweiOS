/* ==============================================================================
 * ZweiOS - Bare-Metal x86_64 Operating System
 * Component: Virtual Memory Manager (VMM 4-Level Paging & HHDM)
 * ============================================================================== */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

namespace mm {

inline constexpr uint64_t KERNEL_VIRT_BASE = 0xFFFFFFFF80000000ULL;
inline constexpr uint64_t HHDM_VIRT_BASE   = 0xFFFF800000000000ULL;
inline constexpr uint64_t HEAP_VIRT_BASE   = 0xFFFFFFFF90000000ULL;
inline constexpr uint64_t HEAP_VIRT_MAX    = 0xFFFFFFFFBFFFFFFFULL;

// Page Table Entry (PTE) Flags
inline constexpr uint64_t PTE_PRESENT   = (1ULL << 0);
inline constexpr uint64_t PTE_WRITABLE  = (1ULL << 1);
inline constexpr uint64_t PTE_USER      = (1ULL << 2);
inline constexpr uint64_t PTE_PWT       = (1ULL << 3);
inline constexpr uint64_t PTE_PCD       = (1ULL << 4);
inline constexpr uint64_t PTE_ACCESSED  = (1ULL << 5);
inline constexpr uint64_t PTE_DIRTY     = (1ULL << 6);
inline constexpr uint64_t PTE_HUGE      = (1ULL << 7);
inline constexpr uint64_t PTE_GLOBAL    = (1ULL << 8);
inline constexpr uint64_t PTE_NX        = (1ULL << 63);
inline constexpr uint64_t PTE_ADDR_MASK = 0x000FFFFFFFFFF000ULL;

struct page_table_t {
    uint64_t entries[512];
} __attribute__((aligned(4096)));

void vmm_init();
bool vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags = (PTE_PRESENT | PTE_WRITABLE));
bool vmm_unmap_page(uint64_t virt);
uint64_t vmm_virt_to_phys(uint64_t virt);
void vmm_flush_tlb(uint64_t virt);

} // namespace mm

using mm::KERNEL_VIRT_BASE;
using mm::HHDM_VIRT_BASE;
using mm::HEAP_VIRT_BASE;
using mm::HEAP_VIRT_MAX;
using mm::PTE_PRESENT;
using mm::PTE_WRITABLE;
using mm::PTE_USER;
using mm::PTE_HUGE;
using mm::PTE_NX;
using mm::vmm_init;
using mm::vmm_map_page;
using mm::vmm_unmap_page;
using mm::vmm_virt_to_phys;
using mm::vmm_flush_tlb;
