/* ==============================================================================
 * ZweiOS - Bare-Metal x86_64 Operating System
 * Architecture: x86_64 Long Mode
 * Component: Interrupt Descriptor Table (IDT)
 * ============================================================================== */

#pragma once

#include <stdint.h>
#include <stddef.h>

namespace arch {

// ==============================================================================
// 64-Bit IDT Entry Structure (16 Bytes per Entry)
// ==============================================================================
struct [[gnu::packed]] idt_entry_t {
    uint16_t offset_low;       // Target ISR address bits 0..15
    uint16_t selector;         // Code segment selector (0x08)
    uint8_t  ist;              // Bits 0..2: IST index (1..7, 0=none); Bits 3..7: Reserved 0
    uint8_t  type_attributes;  // Present (0x80) | DPL (0x00 or 0x60) | Type (0x0E = 64-bit Interrupt)
    uint16_t offset_mid;       // Target ISR address bits 16..31
    uint32_t offset_high;      // Target ISR address bits 32..63
    uint32_t reserved;         // Must be 0
};
static_assert(sizeof(idt_entry_t) == 16, "idt_entry_t must be exactly 16 bytes");

// ==============================================================================
// IDTR Descriptor Pointer Structure for lidt (10 Bytes)
// ==============================================================================
struct [[gnu::packed]] idtr_t {
    uint16_t limit;            // sizeof(idt_table) - 1 = 4095
    uint64_t base;             // Virtual base address of IDT array
};
static_assert(sizeof(idtr_t) == 10, "idtr_t must be exactly 10 bytes");

// Type attribute constants
inline constexpr uint8_t IDT_ATTR_INTERRUPT_GATE = 0x8E; // Present, Ring 0, 64-bit Interrupt Gate
inline constexpr uint8_t IDT_ATTR_TRAP_GATE      = 0x8F; // Present, Ring 0, 64-bit Trap Gate
inline constexpr uint8_t IDT_ATTR_USER_GATE      = 0xEE; // Present, Ring 3, 64-bit Trap/Interrupt Gate

// ==============================================================================
// Public IDT Interface
// ==============================================================================

// Initialize 256-entry IDT, install exception/IRQ assembly stubs, and load IDTR
void idt_init();

// Configure a specific gate descriptor
void idt_set_gate(uint8_t vector, void* isr_stub_addr, uint16_t selector = 0x08, uint8_t type_attr = IDT_ATTR_INTERRUPT_GATE, uint8_t ist = 0);

// Inspect active IDTR register state
const idtr_t& idt_get_idtr();

// Low-level assembly lidt instruction
extern "C" void idt_load(const idtr_t* idtr);

} // namespace arch

using arch::idt_init;
using arch::idt_set_gate;
using arch::idt_get_idtr;
using arch::idt_entry_t;
using arch::idtr_t;
using arch::IDT_ATTR_INTERRUPT_GATE;
using arch::IDT_ATTR_TRAP_GATE;
using arch::IDT_ATTR_USER_GATE;
