/* ==============================================================================
 * ZweiOS - Bare-Metal x86_64 Operating System
 * Architecture: x86_64 Long Mode
 * Component: Global Descriptor Table (GDT) & Task State Segment (TSS) Implementation
 * ============================================================================== */

#include "arch/x86_64/gdt.hpp"
#include "drivers/serial.hpp"

namespace arch {

// Static table and descriptor allocations
alignas(16) static gdt_table_t gdt_table;
alignas(16) static gdtr_t      gdtr;
alignas(16) static tss_t       global_tss;

// Dedicated emergency stack allocations (16 KB each)
alignas(16) static uint8_t double_fault_stack[16384];
alignas(16) static uint8_t kernel_interrupt_stack[16384];

// Helper: Populate standard 8-byte GDT code/data entry
static void set_gdt_entry(size_t index, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags) {
    gdt_table.entries[index].limit_low   = static_cast<uint16_t>(limit & 0xFFFF);
    gdt_table.entries[index].base_low    = static_cast<uint16_t>(base & 0xFFFF);
    gdt_table.entries[index].base_middle = static_cast<uint8_t>((base >> 16) & 0xFF);
    gdt_table.entries[index].access      = access;
    gdt_table.entries[index].granularity = static_cast<uint8_t>(((limit >> 16) & 0x0F) | (flags & 0xF0));
    gdt_table.entries[index].base_high   = static_cast<uint8_t>((base >> 24) & 0xFF);
}

// Helper: Populate 16-byte TSS descriptor entry in GDT (Indices 5 & 6)
static void set_tss_entry(uint64_t base, uint32_t limit) {
    gdt_table.tss_entry.limit_low     = static_cast<uint16_t>(limit & 0xFFFF);
    gdt_table.tss_entry.base_low      = static_cast<uint16_t>(base & 0xFFFF);
    gdt_table.tss_entry.base_mid_low  = static_cast<uint8_t>((base >> 16) & 0xFF);
    gdt_table.tss_entry.access        = 0x89; // Present (0x80) | DPL 0 (0x00) | 64-bit TSS Available (0x09)
    gdt_table.tss_entry.granularity   = static_cast<uint8_t>((limit >> 16) & 0x0F); // G=0 (Byte granularity)
    gdt_table.tss_entry.base_mid_high = static_cast<uint8_t>((base >> 24) & 0xFF);
    gdt_table.tss_entry.base_upper32  = static_cast<uint32_t>((base >> 32) & 0xFFFFFFFF);
    gdt_table.tss_entry.reserved      = 0;
}

void gdt_init() {
    // 1. Clear structures to zero
    uint8_t* gdt_raw = reinterpret_cast<uint8_t*>(&gdt_table);
    for (size_t i = 0; i < sizeof(gdt_table_t); ++i) {
        gdt_raw[i] = 0;
    }

    uint8_t* tss_raw = reinterpret_cast<uint8_t*>(&global_tss);
    for (size_t i = 0; i < sizeof(tss_t); ++i) {
        tss_raw[i] = 0;
    }

    // 2. Configure GDT Segment Entries
    // Index 0: Null Descriptor (0x00)
    set_gdt_entry(0, 0, 0, 0x00, 0x00);

    // Index 1: Kernel Code 64-bit (0x08)
    // Access: 0x9A = Present (1) | DPL 0 (00) | S (1) | Exec (1) | Read (1)
    // Flags:  0xA0 = Granularity 4KB (G=1) | Long Mode (L=1) | D/B (0)
    set_gdt_entry(1, 0, 0xFFFFF, 0x9A, 0xA0);

    // Index 2: Kernel Data 64-bit (0x10)
    // Access: 0x92 = Present (1) | DPL 0 (00) | S (1) | Writable (1)
    // Flags:  0xC0 = Granularity 4KB (G=1) | 32-bit default (D/B=1)
    set_gdt_entry(2, 0, 0xFFFFF, 0x92, 0xC0);

    // Index 3: User Data 64-bit (0x18, RPL3 = 0x1B)
    // Access: 0xF2 = Present (1) | DPL 3 (11) | S (1) | Writable (1)
    // Flags:  0xC0 = Granularity 4KB (G=1) | 32-bit default (D/B=1)
    set_gdt_entry(3, 0, 0xFFFFF, 0xF2, 0xC0);

    // Index 4: User Code 64-bit (0x20, RPL3 = 0x23)
    // Access: 0xFA = Present (1) | DPL 3 (11) | S (1) | Exec (1) | Read (1)
    // Flags:  0xA0 = Granularity 4KB (G=1) | Long Mode (L=1) | D/B (0)
    set_gdt_entry(4, 0, 0xFFFFF, 0xFA, 0xA0);

    // 3. Configure Task State Segment (TSS)
    uint64_t ring0_stack_top = reinterpret_cast<uint64_t>(kernel_interrupt_stack + sizeof(kernel_interrupt_stack));
    uint64_t df_stack_top    = reinterpret_cast<uint64_t>(double_fault_stack + sizeof(double_fault_stack));

    global_tss.rsp0       = ring0_stack_top;
    global_tss.ist1       = df_stack_top;
    global_tss.iomap_base = static_cast<uint16_t>(sizeof(tss_t)); // No I/O permission bitmap

    // 4. Populate 16-byte TSS descriptor in GDT (Indices 5 & 6)
    set_tss_entry(reinterpret_cast<uint64_t>(&global_tss), sizeof(tss_t) - 1);

    // 5. Populate GDTR descriptor pointer
    gdtr.limit = static_cast<uint16_t>(sizeof(gdt_table_t) - 1);
    gdtr.base  = reinterpret_cast<uint64_t>(&gdt_table);

    // 6. Reload Hardware Registers: GDTR, CS (0x08), DS/ES/SS/FS/GS (0x10), and TR (0x28)
    gdt_load(&gdtr, KERNEL_CS, KERNEL_DS, TSS_SEL);

    drivers::serial_puts("[GDT] 64-bit GDT & TSS initialized (Selectors: Code 0x08, Data 0x10, TSS 0x28)\r\n");
}

void gdt_set_kernel_stack(uint64_t stack_top) {
    global_tss.rsp0 = stack_top;
}

void gdt_set_ist(uint8_t ist_index, uint64_t stack_top) {
    if (ist_index >= 1 && ist_index <= 7) {
        uint64_t* ist_array = &global_tss.ist1;
        ist_array[ist_index - 1] = stack_top;
    }
}

const tss_t& gdt_get_tss() {
    return global_tss;
}

const gdtr_t& gdt_get_gdtr() {
    return gdtr;
}

} // namespace arch
