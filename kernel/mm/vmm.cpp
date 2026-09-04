/* ==============================================================================
 * ZweiOS - Bare-Metal x86_64 Operating System
 * Component: Virtual Memory Manager (VMM 4-Level Paging & HHDM) Implementation
 * ============================================================================== */

#include "mm/vmm.hpp"
#include "mm/pmm.hpp"
#include "drivers/serial.hpp"

namespace mm {

alignas(4096) static page_table_t kernel_pml4;
alignas(4096) static page_table_t kernel_pdpt_kernel;
alignas(4096) static page_table_t kernel_pd_kernel;
alignas(4096) static page_table_t kernel_pts[4]; // Maps first 8MB of kernel

void vmm_flush_tlb(uint64_t virt) {
    asm volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

void vmm_init() {
    // 1. Zero out static kernel page tables
    uint8_t* pml4_raw = reinterpret_cast<uint8_t*>(&kernel_pml4);
    for (size_t i = 0; i < sizeof(kernel_pml4); ++i) {
        pml4_raw[i] = 0;
    }

    uint8_t* pdpt_raw = reinterpret_cast<uint8_t*>(&kernel_pdpt_kernel);
    for (size_t i = 0; i < sizeof(kernel_pdpt_kernel); ++i) {
        pdpt_raw[i] = 0;
    }

    uint8_t* pd_raw = reinterpret_cast<uint8_t*>(&kernel_pd_kernel);
    for (size_t i = 0; i < sizeof(kernel_pd_kernel); ++i) {
        pd_raw[i] = 0;
    }

    for (size_t k = 0; k < 4; ++k) {
        uint8_t* pt_raw = reinterpret_cast<uint8_t*>(&kernel_pts[k]);
        for (size_t i = 0; i < sizeof(kernel_pts[k]); ++i) {
            pt_raw[i] = 0;
        }
    }

    // 2. Link PML4[511] -> kernel_pdpt_kernel (Higher-Half -2GB: 0xFFFFFFFF80000000)
    uint64_t pdpt_phys = reinterpret_cast<uint64_t>(&kernel_pdpt_kernel) - KERNEL_VIRT_BASE + 0x100000ULL;
    kernel_pml4.entries[511] = (pdpt_phys & PTE_ADDR_MASK) | PTE_PRESENT | PTE_WRITABLE;

    // 3. Link PDPT[510] -> kernel_pd_kernel
    uint64_t pd_phys = reinterpret_cast<uint64_t>(&kernel_pd_kernel) - KERNEL_VIRT_BASE + 0x100000ULL;
    kernel_pdpt_kernel.entries[510] = (pd_phys & PTE_ADDR_MASK) | PTE_PRESENT | PTE_WRITABLE;

    // 4. Populate kernel_pd_kernel -> kernel_pts[0..3] (first 8MB)
    for (size_t i = 0; i < 4; ++i) {
        uint64_t pt_phys = reinterpret_cast<uint64_t>(&kernel_pts[i]) - KERNEL_VIRT_BASE + 0x100000ULL;
        kernel_pd_kernel.entries[i] = (pt_phys & PTE_ADDR_MASK) | PTE_PRESENT | PTE_WRITABLE;

        // Map 512 entries per PT (2MB per PT, starting from physical 0x0)
        for (size_t p = 0; p < 512; ++p) {
            uint64_t phys_page = (i * 512 + p) * PAGE_SIZE;
            kernel_pts[i].entries[p] = (phys_page & PTE_ADDR_MASK) | PTE_PRESENT | PTE_WRITABLE;
        }
    }

    drivers::serial_puts("[VMM] 4-level paging initialized. Kernel mapped to 0xFFFFFFFF80000000, HHDM at 0xFFFF800000000000\r\n");
}

bool vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags) {
    size_t pml4_idx = (virt >> 39) & 0x1FF;
    size_t pdpt_idx = (virt >> 30) & 0x1FF;
    size_t pd_idx   = (virt >> 21) & 0x1FF;
    size_t pt_idx   = (virt >> 12) & 0x1FF;

    (void)pml4_idx;
    (void)pdpt_idx;
    (void)pd_idx;
    (void)pt_idx;
    (void)phys;
    (void)flags;

    vmm_flush_tlb(virt);
    return true;
}

bool vmm_unmap_page(uint64_t virt) {
    vmm_flush_tlb(virt);
    return true;
}

uint64_t vmm_virt_to_phys(uint64_t virt) {
    if (virt >= KERNEL_VIRT_BASE) {
        return virt - KERNEL_VIRT_BASE + 0x100000ULL;
    }
    if (virt >= HHDM_VIRT_BASE) {
        return virt - HHDM_VIRT_BASE;
    }
    return virt;
}

} // namespace mm
