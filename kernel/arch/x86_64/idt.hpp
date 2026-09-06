

#pragma once

#include <stdint.h>
#include <stddef.h>

namespace arch {




struct [[gnu::packed]] idt_entry_t {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attributes;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
};
static_assert(sizeof(idt_entry_t) == 16, "idt_entry_t must be exactly 16 bytes");




struct [[gnu::packed]] idtr_t {
    uint16_t limit;
    uint64_t base;
};
static_assert(sizeof(idtr_t) == 10, "idtr_t must be exactly 10 bytes");


inline constexpr uint8_t IDT_ATTR_INTERRUPT_GATE = 0x8E;
inline constexpr uint8_t IDT_ATTR_TRAP_GATE      = 0x8F;
inline constexpr uint8_t IDT_ATTR_USER_GATE      = 0xEE;






void idt_init();


void idt_set_gate(uint8_t vector, void* isr_stub_addr, uint16_t selector = 0x08, uint8_t type_attr = IDT_ATTR_INTERRUPT_GATE, uint8_t ist = 0);


const idtr_t& idt_get_idtr();


extern "C" void idt_load(const idtr_t* idtr);

}

using arch::idt_init;
using arch::idt_set_gate;
using arch::idt_get_idtr;
using arch::idt_entry_t;
using arch::idtr_t;
using arch::IDT_ATTR_INTERRUPT_GATE;
using arch::IDT_ATTR_TRAP_GATE;
using arch::IDT_ATTR_USER_GATE;
