

#include "arch/x86_64/gdt.hpp"
#include "drivers/serial.hpp"

namespace arch {


alignas(16) static gdt_table_t gdt_table;
alignas(16) static gdtr_t      gdtr;
alignas(16) static tss_t       global_tss;


alignas(16) static uint8_t double_fault_stack[16384];
alignas(16) static uint8_t kernel_interrupt_stack[16384];


static void set_gdt_entry(size_t index, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags) {
    gdt_table.entries[index].limit_low   = static_cast<uint16_t>(limit & 0xFFFF);
    gdt_table.entries[index].base_low    = static_cast<uint16_t>(base & 0xFFFF);
    gdt_table.entries[index].base_middle = static_cast<uint8_t>((base >> 16) & 0xFF);
    gdt_table.entries[index].access      = access;
    gdt_table.entries[index].granularity = static_cast<uint8_t>(((limit >> 16) & 0x0F) | (flags & 0xF0));
    gdt_table.entries[index].base_high   = static_cast<uint8_t>((base >> 24) & 0xFF);
}


static void set_tss_entry(uint64_t base, uint32_t limit) {
    gdt_table.tss_entry.limit_low     = static_cast<uint16_t>(limit & 0xFFFF);
    gdt_table.tss_entry.base_low      = static_cast<uint16_t>(base & 0xFFFF);
    gdt_table.tss_entry.base_mid_low  = static_cast<uint8_t>((base >> 16) & 0xFF);
    gdt_table.tss_entry.access        = 0x89;
    gdt_table.tss_entry.granularity   = static_cast<uint8_t>((limit >> 16) & 0x0F);
    gdt_table.tss_entry.base_mid_high = static_cast<uint8_t>((base >> 24) & 0xFF);
    gdt_table.tss_entry.base_upper32  = static_cast<uint32_t>((base >> 32) & 0xFFFFFFFF);
    gdt_table.tss_entry.reserved      = 0;
}

void gdt_init() {

    uint8_t* gdt_raw = reinterpret_cast<uint8_t*>(&gdt_table);
    for (size_t i = 0; i < sizeof(gdt_table_t); ++i) {
        gdt_raw[i] = 0;
    }

    uint8_t* tss_raw = reinterpret_cast<uint8_t*>(&global_tss);
    for (size_t i = 0; i < sizeof(tss_t); ++i) {
        tss_raw[i] = 0;
    }



    set_gdt_entry(0, 0, 0, 0x00, 0x00);




    set_gdt_entry(1, 0, 0xFFFFF, 0x9A, 0xA0);




    set_gdt_entry(2, 0, 0xFFFFF, 0x92, 0xC0);




    set_gdt_entry(3, 0, 0xFFFFF, 0xF2, 0xC0);




    set_gdt_entry(4, 0, 0xFFFFF, 0xFA, 0xA0);


    uint64_t ring0_stack_top = reinterpret_cast<uint64_t>(kernel_interrupt_stack + sizeof(kernel_interrupt_stack));
    uint64_t df_stack_top    = reinterpret_cast<uint64_t>(double_fault_stack + sizeof(double_fault_stack));

    global_tss.rsp0       = ring0_stack_top;
    global_tss.ist1       = df_stack_top;
    global_tss.iomap_base = static_cast<uint16_t>(sizeof(tss_t));


    set_tss_entry(reinterpret_cast<uint64_t>(&global_tss), sizeof(tss_t) - 1);


    gdtr.limit = static_cast<uint16_t>(sizeof(gdt_table_t) - 1);
    gdtr.base  = reinterpret_cast<uint64_t>(&gdt_table);


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

}
