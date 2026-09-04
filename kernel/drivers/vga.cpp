/* ==============================================================================
 * ZweiOS - Bare-Metal x86_64 Operating System
 * Component: VGA Text Mode (80x25) & Hardware Cursor Implementation
 * ============================================================================== */

#include "drivers/vga.hpp"
#include "arch/x86_64/io.hpp"
#include "drivers/serial.hpp"

namespace drivers {

static volatile uint16_t* const vga_buffer = reinterpret_cast<volatile uint16_t*>(VGA_BUFFER_VIRT_ADDR);
static size_t cursor_x = 0;
static size_t cursor_y = 0;
static uint8_t current_attribute = 0x07; // Light Grey on Black

static void update_hardware_cursor() {
    uint16_t pos = static_cast<uint16_t>(cursor_y * VGA_WIDTH + cursor_x);
    arch::outb(0x3D4, 0x0F);
    arch::outb(0x3D5, static_cast<uint8_t>(pos & 0xFF));
    arch::outb(0x3D4, 0x0E);
    arch::outb(0x3D5, static_cast<uint8_t>((pos >> 8) & 0xFF));
}

void vga_enable_cursor(uint8_t cursor_start, uint8_t cursor_end) {
    arch::outb(0x3D4, 0x0A);
    arch::outb(0x3D5, (arch::inb(0x3D5) & 0xC0) | cursor_start);
    arch::outb(0x3D4, 0x0B);
    arch::outb(0x3D5, (arch::inb(0x3D5) & 0xE0) | cursor_end);
}

void vga_disable_cursor() {
    arch::outb(0x3D4, 0x0A);
    arch::outb(0x3D5, 0x20);
}

void vga_set_color(uint8_t fg, uint8_t bg) {
    current_attribute = (bg << 4) | (fg & 0x0F);
}

void vga_clear() {
    uint16_t blank = static_cast<uint16_t>(' ') | (static_cast<uint16_t>(current_attribute) << 8);
    for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; ++i) {
        vga_buffer[i] = blank;
    }
    cursor_x = 0;
    cursor_y = 0;
    update_hardware_cursor();
}

static void scroll() {
    for (size_t row = 1; row < VGA_HEIGHT; ++row) {
        for (size_t col = 0; col < VGA_WIDTH; ++col) {
            vga_buffer[(row - 1) * VGA_WIDTH + col] = vga_buffer[row * VGA_WIDTH + col];
        }
    }
    uint16_t blank = static_cast<uint16_t>(' ') | (static_cast<uint16_t>(current_attribute) << 8);
    for (size_t col = 0; col < VGA_WIDTH; ++col) {
        vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + col] = blank;
    }
    cursor_y = VGA_HEIGHT - 1;
}

void vga_putc(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
        if (cursor_y >= VGA_HEIGHT) {
            scroll();
        }
        update_hardware_cursor();
        return;
    }

    if (c == '\r') {
        cursor_x = 0;
        update_hardware_cursor();
        return;
    }

    if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
            size_t idx = cursor_y * VGA_WIDTH + cursor_x;
            vga_buffer[idx] = static_cast<uint16_t>(' ') | (static_cast<uint16_t>(current_attribute) << 8);
        } else if (cursor_y > 0) {
            cursor_y--;
            cursor_x = VGA_WIDTH - 1;
            size_t idx = cursor_y * VGA_WIDTH + cursor_x;
            vga_buffer[idx] = static_cast<uint16_t>(' ') | (static_cast<uint16_t>(current_attribute) << 8);
        }
        update_hardware_cursor();
        return;
    }

    if (c == '\t') {
        cursor_x = (cursor_x + 8) & ~7;
        if (cursor_x >= VGA_WIDTH) {
            cursor_x = 0;
            cursor_y++;
            if (cursor_y >= VGA_HEIGHT) {
                scroll();
            }
        }
        update_hardware_cursor();
        return;
    }

    // Normal printable character
    size_t index = cursor_y * VGA_WIDTH + cursor_x;
    vga_buffer[index] = static_cast<uint16_t>(static_cast<uint8_t>(c)) | (static_cast<uint16_t>(current_attribute) << 8);
    cursor_x++;
    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
        if (cursor_y >= VGA_HEIGHT) {
            scroll();
        }
    }
    update_hardware_cursor();
}

void vga_puts(const char* str) {
    if (!str) return;
    for (size_t i = 0; str[i] != '\0'; ++i) {
        vga_putc(str[i]);
    }
}

void vga_set_cursor(size_t x, size_t y) {
    if (x < VGA_WIDTH) cursor_x = x;
    if (y < VGA_HEIGHT) cursor_y = y;
    update_hardware_cursor();
}

void vga_get_cursor(size_t* x, size_t* y) {
    if (x) *x = cursor_x;
    if (y) *y = cursor_y;
}

void vga_init() {
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_clear();
    vga_enable_cursor(14, 15);
    drivers::serial_puts("[VGA] Console initialized (80x25 text mode at 0xB8000)\r\n");
}

} // namespace drivers
