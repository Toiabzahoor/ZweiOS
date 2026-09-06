

#include "drivers/vga.hpp"
#include "arch/x86_64/io.hpp"
#include "drivers/serial.hpp"
#include "drivers/vbe.hpp"
#include "gui/font.hpp"
#include "gui/wm.hpp"
#include "lib/string.hpp"

namespace drivers {

static volatile uint16_t* const vga_buffer = reinterpret_cast<volatile uint16_t*>(VGA_BUFFER_VIRT_ADDR);
static size_t cursor_x = 0;
static size_t cursor_y = 0;
static uint8_t current_attribute = 0x07;

static const uint32_t vga_to_argb[16] = {
    0xFF0F172A,
    0xFF3B82F6,
    0xFF10B981,
    0xFF06B6D4,
    0xFFEF4444,
    0xFFEC4899,
    0xFFD97706,
    0xFF94A3B8,
    0xFF64748B,
    0xFF60A5FA,
    0xFF34D399,
    0xFF38BDF8,
    0xFFF87171,
    0xFFF472B6,
    0xFFFBBF24,
    0xFFF8FAFC,
};

static size_t get_max_cols() {
    auto* fb = vbe_get_info();
    if (fb && fb->is_active && fb->width > 0) {
        return (fb->width - 16) / 8;
    }
    return VGA_WIDTH;
}

static size_t get_max_rows() {
    auto* fb = vbe_get_info();
    if (fb && fb->is_active && fb->height > 0) {
        return (fb->height - 36) / 16;
    }
    return VGA_HEIGHT;
}

static void fb_draw_char(size_t col, size_t row, char c, uint8_t attr) {
    auto* fb = vbe_get_info();
    if (!fb || !fb->is_active || !fb->front_buffer) return;

    size_t px_x = 8 + (col * 8);
    size_t px_y = 36 + (row * 16);
    if (px_x + 8 > fb->width || px_y + 16 > fb->height) return;

    uint32_t fg = vga_to_argb[attr & 0x0F];
    uint32_t bg = vga_to_argb[(attr >> 4) & 0x0F];

    const uint8_t* glyph = gui::font_get_glyph(static_cast<unsigned char>(c));

    for (size_t r = 0; r < 16; ++r) {
        uint8_t byte = glyph[r];
        size_t fb_row = px_y + r;
        size_t row_offset = fb_row * fb->width + px_x;
        for (size_t bit = 0; bit < 8; ++bit) {
            uint32_t color = (byte & (0x80 >> bit)) ? fg : bg;
            if (fb->back_buffer) {
                fb->back_buffer[row_offset + bit] = color;
            }
            fb->front_buffer[row_offset + bit] = color;
        }
    }
}

static void fb_scroll() {
    auto* fb = vbe_get_info();
    if (!fb || !fb->is_active || !fb->front_buffer) return;

    size_t top_y = 36;
    size_t max_rows = (fb->height - 36) / 16;
    size_t bytes_per_line = fb->width * 4;

    size_t lines_to_copy = (max_rows - 1) * 16;
    size_t src_offset = (top_y + 16) * fb->width;
    size_t dst_offset = top_y * fb->width;

    if (fb->back_buffer) {
        lib::memmove(&fb->back_buffer[dst_offset], &fb->back_buffer[src_offset], lines_to_copy * bytes_per_line);
    }
    lib::memmove(&fb->front_buffer[dst_offset], &fb->front_buffer[src_offset], lines_to_copy * bytes_per_line);

    uint32_t bg = vga_to_argb[0];
    size_t last_row_start = (top_y + lines_to_copy) * fb->width;
    for (size_t i = 0; i < 16 * fb->width; ++i) {
        if (fb->back_buffer) fb->back_buffer[last_row_start + i] = bg;
        fb->front_buffer[last_row_start + i] = bg;
    }
}

static void update_hardware_cursor() {
    auto* fb = vbe_get_info();
    if (fb && fb->is_active) {
        return;
    }

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
    if (gui::wm_is_running()) return;
    auto* fb = vbe_get_info();
    if (fb && fb->is_active && fb->front_buffer) {
        uint32_t bg = vga_to_argb[0];
        size_t total = fb->width * fb->height;
        for (size_t i = 0; i < total; ++i) {
            if (fb->back_buffer) fb->back_buffer[i] = bg;
            fb->front_buffer[i] = bg;
        }

        for (size_t y = 0; y < 30; ++y) {
            for (size_t x = 0; x < fb->width; ++x) {
                uint32_t banner_bg = 0xFF1E293B;
                if (fb->back_buffer) fb->back_buffer[y * fb->width + x] = banner_bg;
                fb->front_buffer[y * fb->width + x] = banner_bg;
            }
        }
        for (size_t x = 0; x < fb->width; ++x) {
            uint32_t border_color = 0xFF334155;
            if (fb->back_buffer) fb->back_buffer[30 * fb->width + x] = border_color;
            fb->front_buffer[30 * fb->width + x] = border_color;
        }

        const char* title = "ZweiOS Universal Dual-OS Console [1920x1080 32-bit True Color] - made by toiabzahoor";
        for (size_t i = 0; title[i] != '\0'; ++i) {
            const uint8_t* glyph = gui::font_get_glyph(static_cast<unsigned char>(title[i]));
            size_t px_x = 16 + i * 8;
            size_t px_y = 7;
            for (size_t r = 0; r < 16; ++r) {
                uint8_t byte = glyph[r];
                for (size_t bit = 0; bit < 8; ++bit) {
                    if (byte & (0x80 >> bit)) {
                        size_t offset = (px_y + r) * fb->width + (px_x + bit);
                        if (fb->back_buffer) fb->back_buffer[offset] = 0xFFFFFFFF;
                        fb->front_buffer[offset] = 0xFFFFFFFF;
                    }
                }
            }
        }
    }

    uint16_t blank = static_cast<uint16_t>(' ') | (static_cast<uint16_t>(current_attribute) << 8);
    for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; ++i) {
        vga_buffer[i] = blank;
    }
    cursor_x = 0;
    cursor_y = 0;
    update_hardware_cursor();
}

static void scroll() {
    auto* fb = vbe_get_info();
    if (fb && fb->is_active) {
        fb_scroll();
        size_t max_r = get_max_rows();
        cursor_y = (max_r > 0) ? max_r - 1 : 0;
        return;
    }

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
    if (gui::wm_is_running()) return;
    size_t max_cols = get_max_cols();
    size_t max_rows = get_max_rows();

    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
        if (cursor_y >= max_rows) {
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
            fb_draw_char(cursor_x, cursor_y, ' ', current_attribute);
            if (cursor_x < VGA_WIDTH && cursor_y < VGA_HEIGHT) {
                size_t idx = cursor_y * VGA_WIDTH + cursor_x;
                vga_buffer[idx] = static_cast<uint16_t>(' ') | (static_cast<uint16_t>(current_attribute) << 8);
            }
        } else if (cursor_y > 0) {
            cursor_y--;
            cursor_x = max_cols - 1;
            fb_draw_char(cursor_x, cursor_y, ' ', current_attribute);
            if (cursor_x < VGA_WIDTH && cursor_y < VGA_HEIGHT) {
                size_t idx = cursor_y * VGA_WIDTH + cursor_x;
                vga_buffer[idx] = static_cast<uint16_t>(' ') | (static_cast<uint16_t>(current_attribute) << 8);
            }
        }
        update_hardware_cursor();
        return;
    }

    if (c == '\t') {
        cursor_x = (cursor_x + 8) & ~7;
        if (cursor_x >= max_cols) {
            cursor_x = 0;
            cursor_y++;
            if (cursor_y >= max_rows) {
                scroll();
            }
        }
        update_hardware_cursor();
        return;
    }

    fb_draw_char(cursor_x, cursor_y, c, current_attribute);

    if (cursor_x < VGA_WIDTH && cursor_y < VGA_HEIGHT) {
        size_t index = cursor_y * VGA_WIDTH + cursor_x;
        vga_buffer[index] = static_cast<uint16_t>(static_cast<uint8_t>(c)) | (static_cast<uint16_t>(current_attribute) << 8);
    }

    cursor_x++;
    if (cursor_x >= max_cols) {
        cursor_x = 0;
        cursor_y++;
        if (cursor_y >= max_rows) {
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
    size_t max_cols = get_max_cols();
    size_t max_rows = get_max_rows();
    if (x < max_cols) cursor_x = x;
    if (y < max_rows) cursor_y = y;
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
    drivers::serial_puts("[VGA] High-Resolution 1920x1080 Framebuffer Console Initialized.\r\n");
}

void vga_restore_text_mode() {
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_clear();
}

}
