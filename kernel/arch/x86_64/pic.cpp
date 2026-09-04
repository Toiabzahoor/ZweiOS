/* ==============================================================================
 * ZweiOS - Bare-Metal x86_64 Operating System
 * Architecture: x86_64 Long Mode
 * Component: 8259 Programmable Interrupt Controller (PIC) Implementation
 * ============================================================================== */

#include "arch/x86_64/pic.hpp"
#include "arch/x86_64/io.hpp"
#include "drivers/serial.hpp"

namespace arch {

void pic_remap(uint8_t offset1, uint8_t offset2) {
    // 1. Save current interrupt masks
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);

    // 2. ICW1: Start initialization sequence in cascade mode
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();

    // 3. ICW2: Remap Master to offset1 (0x20) and Slave to offset2 (0x28)
    outb(PIC1_DATA, offset1);
    io_wait();
    outb(PIC2_DATA, offset2);
    io_wait();

    // 4. ICW3: Tell Master that Slave is attached to IRQ2 (bit 2 = 0x04)
    outb(PIC1_DATA, 0x04);
    io_wait();
    // Tell Slave its cascade identity (IRQ2 = 0x02)
    outb(PIC2_DATA, 0x02);
    io_wait();

    // 5. ICW4: Set 8086 mode
    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    // 6. Restore original interrupt masks
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}

void pic_init() {
    // Remap PIC to avoid conflict with CPU exceptions 0x00-0x1F
    pic_remap(PIC1_OFFSET, PIC2_OFFSET);

    // Unmask timer (IRQ0), keyboard (IRQ1), and cascade (IRQ2)
    // Mask all other IRQs by default
    outb(PIC1_DATA, 0b11111000); // IRQ 0 (timer), IRQ 1 (kbd), IRQ 2 (cascade) enabled
    outb(PIC2_DATA, 0b11111111); // All slave IRQs masked initially

    drivers::serial_puts("[PIC] 8259 PIC remapped (Master: 0x20-0x27, Slave: 0x28-0x2F)\r\n");
}

void pic_send_eoi(uint8_t irq) {
    // If interrupt originated from Slave PIC (IRQ 8..15), send EOI to both Slave and Master
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
}

void pic_set_mask(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    uint8_t bit  = (irq < 8) ? irq : (irq - 8);
    outb(port, inb(port) | static_cast<uint8_t>(1 << bit));
}

void pic_clear_mask(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    uint8_t bit  = (irq < 8) ? irq : (irq - 8);
    outb(port, inb(port) & static_cast<uint8_t>(~(1 << bit)));
}

void pic_disable() {
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

static uint16_t pic_get_irq_reg(uint8_t ocw3) {
    outb(PIC1_COMMAND, ocw3);
    outb(PIC2_COMMAND, ocw3);
    return static_cast<uint16_t>(inb(PIC1_COMMAND)) | (static_cast<uint16_t>(inb(PIC2_COMMAND)) << 8);
}

uint16_t pic_get_irr() {
    return pic_get_irq_reg(PIC_READ_IRR);
}

uint16_t pic_get_isr() {
    return pic_get_irq_reg(PIC_READ_ISR);
}

} // namespace arch
