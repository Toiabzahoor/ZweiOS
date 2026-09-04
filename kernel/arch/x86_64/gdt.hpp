/* ==============================================================================
 * ZweiOS - Bare-Metal x86_64 Operating System
 * Architecture: x86_64 Long Mode
 * Component: Global Descriptor Table (GDT) & Task State Segment (TSS)
 * ============================================================================== */

#pragma once

#include <stdint.h>
#include <stddef.h>

namespace arch {

// ==============================================================================
// GDT Segment Selectors
// ==============================================================================
inline constexpr uint16_t GDT_NULL_SELECTOR        = 0x00;
inline constexpr uint16_t GDT_KERNEL_CODE_SELECTOR = 0x08;
inline constexpr uint16_t GDT_KERNEL_DATA_SELECTOR = 0x10;
inline constexpr uint16_t GDT_USER_DATA_SELECTOR   = 0x18;
inline constexpr uint16_t GDT_USER_CODE_SELECTOR   = 0x20;
inline constexpr uint16_t GDT_TSS_SELECTOR         = 0x28;

// Selectors with Requested Privilege Level (RPL)
inline constexpr uint16_t KERNEL_CS = GDT_KERNEL_CODE_SELECTOR | 0; // 0x08
inline constexpr uint16_t KERNEL_DS = GDT_KERNEL_DATA_SELECTOR | 0; // 0x10
inline constexpr uint16_t USER_DS   = GDT_USER_DATA_SELECTOR   | 3; // 0x1B
inline constexpr uint16_t USER_CS   = GDT_USER_CODE_SELECTOR   | 3; // 0x23
inline constexpr uint16_t TSS_SEL   = GDT_TSS_SELECTOR         | 0; // 0x28

// ==============================================================================
// 64-Bit GDT Entry Structure (8 Bytes)
// ==============================================================================
struct [[gnu::packed]] gdt_entry_t {
    uint16_t limit_low;     // Bits 0..15 of segment limit
    uint16_t base_low;      // Bits 0..15 of base address
    uint8_t  base_middle;   // Bits 16..23 of base address
    uint8_t  access;        // Access flags (P, DPL, S, Type)
    uint8_t  granularity;   // Flags (G, D/B, L, AVL) in upper 4 bits, Limit 16..19 in lower 4 bits
    uint8_t  base_high;     // Bits 24..31 of base address
};
static_assert(sizeof(gdt_entry_t) == 8, "gdt_entry_t must be exactly 8 bytes");

// ==============================================================================
// 64-Bit TSS Descriptor Entry Structure (16 Bytes in Long Mode)
// ==============================================================================
struct [[gnu::packed]] gdt_tss_entry_t {
    uint16_t limit_low;     // Bits 0..15 of TSS limit
    uint16_t base_low;      // Bits 0..15 of TSS base address
    uint8_t  base_mid_low;  // Bits 16..23 of TSS base address
    uint8_t  access;        // 0x89 = Present | DPL 0 | 64-bit TSS (Available)
    uint8_t  granularity;   // Flags & limit 16..19
    uint8_t  base_mid_high; // Bits 24..31 of TSS base address
    uint32_t base_upper32;  // Bits 32..63 of TSS base address
    uint32_t reserved;      // Reserved (must be 0)
};
static_assert(sizeof(gdt_tss_entry_t) == 16, "gdt_tss_entry_t must be exactly 16 bytes");

// ==============================================================================
// GDTR Descriptor Pointer Structure for lgdt (10 Bytes)
// ==============================================================================
struct [[gnu::packed]] gdtr_t {
    uint16_t limit;         // sizeof(gdt_table_t) - 1
    uint64_t base;          // 64-bit virtual base address of GDT table
};
static_assert(sizeof(gdtr_t) == 10, "gdtr_t must be exactly 10 bytes");

// ==============================================================================
// 64-Bit Task State Segment (TSS) Structure (104 Bytes)
// ==============================================================================
struct [[gnu::packed]] tss_t {
    uint32_t reserved0;
    uint64_t rsp0;          // Ring 0 Stack Pointer (used on privilege escalation)
    uint64_t rsp1;          // Ring 1 Stack Pointer (unused in 64-bit mode)
    uint64_t rsp2;          // Ring 2 Stack Pointer (unused in 64-bit mode)
    uint64_t reserved1;
    uint64_t ist1;          // Interrupt Stack Table 1 (Emergency Double Fault stack)
    uint64_t ist2;          // Interrupt Stack Table 2 (NMI stack)
    uint64_t ist3;          // Interrupt Stack Table 3 (Machine Check stack)
    uint64_t ist4;          // IST 4
    uint64_t ist5;          // IST 5
    uint64_t ist6;          // IST 6
    uint64_t ist7;          // IST 7
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;    // I/O Permission Bitmap Base Offset (sizeof(tss_t) if none)
};
static_assert(sizeof(tss_t) == 104, "tss_t must be exactly 104 bytes");

// ==============================================================================
// Complete GDT Table (56 Bytes)
// ==============================================================================
struct [[gnu::packed]] gdt_table_t {
    gdt_entry_t entries[5];      // 0: Null, 1: Kernel Code, 2: Kernel Data, 3: User Data, 4: User Code
    gdt_tss_entry_t tss_entry;   // 5-6: 64-bit TSS Descriptor (16 bytes)
};
static_assert(sizeof(gdt_table_t) == 56, "gdt_table_t must be exactly 56 bytes");

// ==============================================================================
// Public GDT & TSS Interface
// ==============================================================================

// Initialize GDT, populate TSS, configure RSP0 / IST1, and reload GDTR / TR
void gdt_init();

// Dynamically update Ring 0 kernel stack pointer (RSP0)
void gdt_set_kernel_stack(uint64_t stack_top);

// Dynamically set an IST emergency stack pointer (1 <= ist_index <= 7)
void gdt_set_ist(uint8_t ist_index, uint64_t stack_top);

// Inspect active TSS state
const tss_t& gdt_get_tss();

// Inspect active GDTR register state
const gdtr_t& gdt_get_gdtr();

// Low-level assembly reload routine (implemented in gdt_flush.S)
extern "C" void gdt_load(const gdtr_t* gdtr, uint16_t code_sel, uint16_t data_sel, uint16_t tss_sel);

} // namespace arch

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
