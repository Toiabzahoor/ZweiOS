

#include "arch/x86_64/idt.hpp"
#include "arch/x86_64/gdt.hpp"
#include "drivers/serial.hpp"


extern "C" {
    void isr_stub_0();
    void isr_stub_1();
    void isr_stub_2();
    void isr_stub_3();
    void isr_stub_4();
    void isr_stub_5();
    void isr_stub_6();
    void isr_stub_7();
    void isr_stub_8();
    void isr_stub_9();
    void isr_stub_10();
    void isr_stub_11();
    void isr_stub_12();
    void isr_stub_13();
    void isr_stub_14();
    void isr_stub_15();
    void isr_stub_16();
    void isr_stub_17();
    void isr_stub_18();
    void isr_stub_19();
    void isr_stub_20();
    void isr_stub_21();
    void isr_stub_22();
    void isr_stub_23();
    void isr_stub_24();
    void isr_stub_25();
    void isr_stub_26();
    void isr_stub_27();
    void isr_stub_28();
    void isr_stub_29();
    void isr_stub_30();
    void isr_stub_31();

    void isr_stub_32();
    void isr_stub_33();
    void isr_stub_34();
    void isr_stub_35();
    void isr_stub_36();
    void isr_stub_37();
    void isr_stub_38();
    void isr_stub_39();
    void isr_stub_40();
    void isr_stub_41();
    void isr_stub_42();
    void isr_stub_43();
    void isr_stub_44();
    void isr_stub_45();
    void isr_stub_46();
    void isr_stub_47();

    void isr_stub_128();
    void isr_stub_255();
}

namespace arch {

alignas(16) static idt_entry_t idt_table[256];
alignas(16) static idtr_t      idtr;

static void* const isr_stub_table[48] = {
    reinterpret_cast<void*>(isr_stub_0),
    reinterpret_cast<void*>(isr_stub_1),
    reinterpret_cast<void*>(isr_stub_2),
    reinterpret_cast<void*>(isr_stub_3),
    reinterpret_cast<void*>(isr_stub_4),
    reinterpret_cast<void*>(isr_stub_5),
    reinterpret_cast<void*>(isr_stub_6),
    reinterpret_cast<void*>(isr_stub_7),
    reinterpret_cast<void*>(isr_stub_8),
    reinterpret_cast<void*>(isr_stub_9),
    reinterpret_cast<void*>(isr_stub_10),
    reinterpret_cast<void*>(isr_stub_11),
    reinterpret_cast<void*>(isr_stub_12),
    reinterpret_cast<void*>(isr_stub_13),
    reinterpret_cast<void*>(isr_stub_14),
    reinterpret_cast<void*>(isr_stub_15),
    reinterpret_cast<void*>(isr_stub_16),
    reinterpret_cast<void*>(isr_stub_17),
    reinterpret_cast<void*>(isr_stub_18),
    reinterpret_cast<void*>(isr_stub_19),
    reinterpret_cast<void*>(isr_stub_20),
    reinterpret_cast<void*>(isr_stub_21),
    reinterpret_cast<void*>(isr_stub_22),
    reinterpret_cast<void*>(isr_stub_23),
    reinterpret_cast<void*>(isr_stub_24),
    reinterpret_cast<void*>(isr_stub_25),
    reinterpret_cast<void*>(isr_stub_26),
    reinterpret_cast<void*>(isr_stub_27),
    reinterpret_cast<void*>(isr_stub_28),
    reinterpret_cast<void*>(isr_stub_29),
    reinterpret_cast<void*>(isr_stub_30),
    reinterpret_cast<void*>(isr_stub_31),
    reinterpret_cast<void*>(isr_stub_32),
    reinterpret_cast<void*>(isr_stub_33),
    reinterpret_cast<void*>(isr_stub_34),
    reinterpret_cast<void*>(isr_stub_35),
    reinterpret_cast<void*>(isr_stub_36),
    reinterpret_cast<void*>(isr_stub_37),
    reinterpret_cast<void*>(isr_stub_38),
    reinterpret_cast<void*>(isr_stub_39),
    reinterpret_cast<void*>(isr_stub_40),
    reinterpret_cast<void*>(isr_stub_41),
    reinterpret_cast<void*>(isr_stub_42),
    reinterpret_cast<void*>(isr_stub_43),
    reinterpret_cast<void*>(isr_stub_44),
    reinterpret_cast<void*>(isr_stub_45),
    reinterpret_cast<void*>(isr_stub_46),
    reinterpret_cast<void*>(isr_stub_47)
};

void idt_set_gate(uint8_t vector, void* isr_stub_addr, uint16_t selector, uint8_t type_attr, uint8_t ist) {
    uint64_t addr = reinterpret_cast<uint64_t>(isr_stub_addr);
    idt_table[vector].offset_low      = static_cast<uint16_t>(addr & 0xFFFF);
    idt_table[vector].selector        = selector;
    idt_table[vector].ist             = ist & 0x07;
    idt_table[vector].type_attributes = type_attr;
    idt_table[vector].offset_mid      = static_cast<uint16_t>((addr >> 16) & 0xFFFF);
    idt_table[vector].offset_high     = static_cast<uint32_t>((addr >> 32) & 0xFFFFFFFF);
    idt_table[vector].reserved        = 0;
}

void idt_init() {

    uint8_t* raw = reinterpret_cast<uint8_t*>(idt_table);
    for (size_t i = 0; i < sizeof(idt_table); ++i) {
        raw[i] = 0;
    }


    for (uint8_t vec = 0; vec < 48; ++vec) {

        uint8_t ist_val = (vec == 8) ? 1 : 0;
        idt_set_gate(vec, isr_stub_table[vec], KERNEL_CS, IDT_ATTR_INTERRUPT_GATE, ist_val);
    }


    idt_set_gate(128, reinterpret_cast<void*>(isr_stub_128), KERNEL_CS, IDT_ATTR_USER_GATE, 0);


    idt_set_gate(255, reinterpret_cast<void*>(isr_stub_255), KERNEL_CS, IDT_ATTR_INTERRUPT_GATE, 0);


    idtr.limit = static_cast<uint16_t>(sizeof(idt_table) - 1);
    idtr.base  = reinterpret_cast<uint64_t>(idt_table);


    idt_load(&idtr);

    drivers::serial_puts("[IDT] 256-entry IDT loaded; Exception ISRs registered\r\n");
}

const idtr_t& idt_get_idtr() {
    return idtr;
}

}
