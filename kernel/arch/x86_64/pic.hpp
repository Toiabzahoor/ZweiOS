

#pragma once

#include <stdint.h>

namespace arch {


inline constexpr uint16_t PIC1_COMMAND = 0x20;
inline constexpr uint16_t PIC1_DATA    = 0x21;
inline constexpr uint16_t PIC2_COMMAND = 0xA0;
inline constexpr uint16_t PIC2_DATA    = 0xA1;


inline constexpr uint8_t ICW1_INIT    = 0x10;
inline constexpr uint8_t ICW1_ICW4    = 0x01;
inline constexpr uint8_t ICW4_8086    = 0x01;
inline constexpr uint8_t PIC_EOI      = 0x20;


inline constexpr uint8_t PIC_READ_IRR = 0x0A;
inline constexpr uint8_t PIC_READ_ISR = 0x0B;


inline constexpr uint8_t PIC1_OFFSET  = 0x20;
inline constexpr uint8_t PIC2_OFFSET  = 0x28;


void pic_init();
void pic_remap(uint8_t offset1 = PIC1_OFFSET, uint8_t offset2 = PIC2_OFFSET);
void pic_send_eoi(uint8_t irq);
void pic_set_mask(uint8_t irq);
void pic_clear_mask(uint8_t irq);
void pic_disable();
uint16_t pic_get_irr();
uint16_t pic_get_isr();

}

using arch::pic_init;
using arch::pic_remap;
using arch::pic_send_eoi;
using arch::pic_set_mask;
using arch::pic_clear_mask;
using arch::pic_disable;
using arch::pic_get_irr;
using arch::pic_get_isr;
