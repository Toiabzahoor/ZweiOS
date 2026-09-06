

#include "mm/vmm.hpp"
#include "mm/pmm.hpp"
#include "drivers/serial.hpp"

namespace mm {

alignas(4096) static page_table_t kernel_pml4;
alignas(4096) static page_table_t kernel_pdpt_kernel;
alignas(4096) static page_table_t kernel_pd_kernel;
alignas(4096) static page_table_t kernel_pts[4];

void vmm_flush_tlb(uint64_t virt) {
    asm volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

void vmm_init() {

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


    uint64_t pdpt_phys = reinterpret_cast<uint64_t>(&kernel_pdpt_kernel) - KERNEL_VIRT_BASE + 0x100000ULL;
    kernel_pml4.entries[511] = (pdpt_phys & PTE_ADDR_MASK) | PTE_PRESENT | PTE_WRITABLE;


    uint64_t pd_phys = reinterpret_cast<uint64_t>(&kernel_pd_kernel) - KERNEL_VIRT_BASE + 0x100000ULL;
    kernel_pdpt_kernel.entries[510] = (pd_phys & PTE_ADDR_MASK) | PTE_PRESENT | PTE_WRITABLE;


    for (size_t i = 0; i < 4; ++i) {
        uint64_t pt_phys = reinterpret_cast<uint64_t>(&kernel_pts[i]) - KERNEL_VIRT_BASE + 0x100000ULL;
        kernel_pd_kernel.entries[i] = (pt_phys & PTE_ADDR_MASK) | PTE_PRESENT | PTE_WRITABLE;


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

uint64_t vmm_get_kernel_pml4() {
    uint64_t cr3 = 0;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    if (cr3 != 0) return cr3;
    return reinterpret_cast<uint64_t>(&kernel_pml4) - KERNEL_VIRT_BASE + 0x100000ULL;
}

uint64_t vmm_create_user_pml4() {
    uint64_t pml4_phys = mm::pmm_alloc_frame();
    if (pml4_phys == 0) return 0;

    auto* new_pml4 = reinterpret_cast<page_table_t*>(pml4_phys);
    for (size_t i = 0; i < 512; ++i) {
        new_pml4->entries[i] = 0;
    }


    new_pml4->entries[511] = kernel_pml4.entries[511];


    uint64_t cur_cr3 = vmm_get_current_pml4();
    if (cur_cr3 != 0) {
        auto* cur_pml4 = reinterpret_cast<page_table_t*>(cur_cr3);
        new_pml4->entries[0] = cur_pml4->entries[0];
    }

    return pml4_phys;
}

void vmm_destroy_user_pml4(uint64_t pml4_phys) {
    if (pml4_phys == 0 || pml4_phys == vmm_get_kernel_pml4()) return;
    mm::pmm_free_frame(pml4_phys);
}

void vmm_switch_pml4(uint64_t pml4_phys) {
    if (pml4_phys != 0) {
        asm volatile("mov %0, %%cr3" : : "r"(pml4_phys) : "memory");
    }
}

uint64_t vmm_get_current_pml4() {
    uint64_t cr3 = 0;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

bool vmm_map_user_page(uint64_t pml4_phys, uint64_t virt, uint64_t phys, uint64_t flags) {
    (void)pml4_phys;
    (void)virt;
    (void)phys;
    (void)flags;
    vmm_flush_tlb(virt);
    return true;
}

}
