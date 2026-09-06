

#pragma once

#include <stdint.h>
#include <stddef.h>

namespace arch {




inline constexpr uint16_t GDT_NULL_SELECTOR        = 0x00;
inline constexpr uint16_t GDT_KERNEL_CODE_SELECTOR = 0x08;
inline constexpr uint16_t GDT_KERNEL_DATA_SELECTOR = 0x10;
inline constexpr uint16_t GDT_USER_DATA_SELECTOR   = 0x18;
inline constexpr uint16_t GDT_USER_CODE_SELECTOR   = 0x20;
inline constexpr uint16_t GDT_TSS_SELECTOR         = 0x28;


inline constexpr uint16_t KERNEL_CS = GDT_KERNEL_CODE_SELECTOR | 0;
inline constexpr uint16_t KERNEL_DS = GDT_KERNEL_DATA_SELECTOR | 0;
inline constexpr uint16_t USER_DS   = GDT_USER_DATA_SELECTOR   | 3;
inline constexpr uint16_t USER_CS   = GDT_USER_CODE_SELECTOR   | 3;
inline constexpr uint16_t TSS_SEL   = GDT_TSS_SELECTOR         | 0;




struct [[gnu::packed]] gdt_entry_t {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
};
static_assert(sizeof(gdt_entry_t) == 8, "gdt_entry_t must be exactly 8 bytes");




struct [[gnu::packed]] gdt_tss_entry_t {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid_low;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_mid_high;
    uint32_t base_upper32;
    uint32_t reserved;
};
static_assert(sizeof(gdt_tss_entry_t) == 16, "gdt_tss_entry_t must be exactly 16 bytes");




struct [[gnu::packed]] gdtr_t {
    uint16_t limit;
    uint64_t base;
};
static_assert(sizeof(gdtr_t) == 10, "gdtr_t must be exactly 10 bytes");




struct [[gnu::packed]] tss_t {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
};
static_assert(sizeof(tss_t) == 104, "tss_t must be exactly 104 bytes");




struct [[gnu::packed]] gdt_table_t {
    gdt_entry_t entries[5];
    gdt_tss_entry_t tss_entry;
};
static_assert(sizeof(gdt_table_t) == 56, "gdt_table_t must be exactly 56 bytes");






void gdt_init();


void gdt_set_kernel_stack(uint64_t stack_top);


void gdt_set_ist(uint8_t ist_index, uint64_t stack_top);


const tss_t& gdt_get_tss();


const gdtr_t& gdt_get_gdtr();


extern "C" void gdt_load(const gdtr_t* gdtr, uint16_t code_sel, uint16_t data_sel, uint16_t tss_sel);

}

using arch::gdt_init;
using arch::gdt_set_kernel_stack;
using arch::gdt_set_ist;
using arch::gdt_get_tss;
using arch::gdt_get_gdtr;
using arch::gdt_entry_t;
using arch::gdt_tss_entry_t;
using arch::gdtr_t;
using arch::tss_t;
using arch::gdt_table_t;
using arch::KERNEL_CS;
using arch::KERNEL_DS;
using arch::USER_DS;
using arch::USER_CS;
using arch::TSS_SEL;
