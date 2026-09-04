#include "drivers/serial.hpp"
#include "arch/x86_64/io.hpp"

namespace drivers {

void serial_init() {
    arch::outb(COM1_PORT + 1, 0x00);    // Disable all UART interrupts
    arch::outb(COM1_PORT + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    arch::outb(COM1_PORT + 0, 0x01);    // Set divisor to 1 (115200 baud low byte)
    arch::outb(COM1_PORT + 1, 0x00);    //                       (high byte)
    arch::outb(COM1_PORT + 3, 0x03);    // 8 bits, no parity, one stop bit (8N1)
    arch::outb(COM1_PORT + 2, 0xC7);    // Enable FIFO, clear TX/RX, 14-byte threshold
    arch::outb(COM1_PORT + 4, 0x0B);    // Enable IRQs, set RTS/DSR

    serial_puts("[COM1] 16550 UART initialized (Port: 0x3F8, Baud: 115200, 8N1)\r\n");
}

static inline int serial_is_transmit_empty() {
    return arch::inb(COM1_PORT + 5) & 0x20;
}

static inline int serial_received() {
    return arch::inb(COM1_PORT + 5) & 0x01;
}

void serial_putc(char c) {
    if (c == '\n') {
        while (!serial_is_transmit_empty());
        arch::outb(COM1_PORT, '\r');
    }
    while (!serial_is_transmit_empty());
    arch::outb(COM1_PORT, static_cast<uint8_t>(c));
}

void serial_puts(const char* str) {
    if (!str) return;
    for (size_t i = 0; str[i] != '\0'; ++i) {
        serial_putc(str[i]);
    }
}

void serial_put_hex64(uint64_t val) {
    const char hex_chars[] = "0123456789ABCDEF";
    for (int i = 60; i >= 0; i -= 4) {
        serial_putc(hex_chars[(val >> i) & 0xF]);
    }
}

void serial_put_hex(uint64_t val) {
    serial_puts("0x");
    serial_put_hex64(val);
}

void serial_put_dec(uint64_t val) {
    if (val == 0) {
        serial_putc('0');
        return;
    }
    char buf[24];
    int pos = 0;
    while (val > 0) {
        buf[pos++] = static_cast<char>('0' + (val % 10));
        val /= 10;
    }
    for (int i = pos - 1; i >= 0; --i) {
        serial_putc(buf[i]);
    }
}

bool serial_try_getc(char* out) {
    if (serial_received()) {
        if (out) {
            *out = static_cast<char>(arch::inb(COM1_PORT));
        }
        return true;
    }
    return false;
}

} // namespace drivers
