

#pragma once

#include <stdint.h>
#include <stddef.h>

namespace drivers {


enum vga_color : uint8_t {
    VGA_COLOR_BLACK         = 0,
    VGA_COLOR_BLUE          = 1,
    VGA_COLOR_GREEN         = 2,
    VGA_COLOR_CYAN          = 3,
    VGA_COLOR_RED           = 4,
    VGA_COLOR_MAGENTA       = 5,
    VGA_COLOR_BROWN         = 6,
    VGA_COLOR_LIGHT_GREY    = 7,
    VGA_COLOR_DARK_GREY     = 8,
    VGA_COLOR_LIGHT_BLUE    = 9,
    VGA_COLOR_LIGHT_GREEN   = 10,
    VGA_COLOR_LIGHT_CYAN    = 11,
    VGA_COLOR_LIGHT_RED     = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN   = 14,
    VGA_COLOR_WHITE         = 15,
};

inline constexpr size_t VGA_WIDTH  = 80;
inline constexpr size_t VGA_HEIGHT = 25;
inline constexpr uint64_t VGA_BUFFER_VIRT_ADDR = 0xFFFFFFFF800B8000ULL;

void vga_init();
void vga_clear();
void vga_putc(char c);
void vga_puts(const char* str);
void vga_set_color(uint8_t fg, uint8_t bg);
void vga_set_cursor(size_t x, size_t y);
void vga_get_cursor(size_t* x, size_t* y);
void vga_enable_cursor(uint8_t cursor_start = 14, uint8_t cursor_end = 15);
void vga_disable_cursor();
void vga_restore_text_mode();

}

using drivers::vga_init;
using drivers::vga_clear;
using drivers::vga_putc;
using drivers::vga_puts;
using drivers::vga_set_color;
using drivers::vga_set_cursor;
using drivers::vga_get_cursor;
using drivers::vga_restore_text_mode;
