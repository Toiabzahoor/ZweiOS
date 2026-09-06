#include "gui/gfx.hpp"
#include "gui/font.hpp"
#include "drivers/vbe.hpp"
#include "lib/string.hpp"

namespace gui {

void gfx_init(void) {
}

uint32_t gfx_alpha_blend(uint32_t bg, uint32_t fg, uint8_t alpha) {
    if (alpha == 255) return fg;
    if (alpha == 0) return bg;

    uint32_t inv_a = 255 - alpha;

    uint32_t fg_r = (fg >> 16) & 0xFF;
    uint32_t fg_g = (fg >> 8) & 0xFF;
    uint32_t fg_b = fg & 0xFF;

    uint32_t bg_r = (bg >> 16) & 0xFF;
    uint32_t bg_g = (bg >> 8) & 0xFF;
    uint32_t bg_b = bg & 0xFF;

    uint32_t r = ((fg_r * alpha) + (bg_r * inv_a)) / 255;
    uint32_t g = ((fg_g * alpha) + (bg_g * inv_a)) / 255;
    uint32_t b = ((fg_b * alpha) + (bg_b * inv_a)) / 255;

    return 0xFF000000 | (r << 16) | (g << 8) | b;
}

static void get_target_info(uint32_t*& target, int& target_w, int& target_h) {
    if (!target) {
        auto* fb = drivers::vbe_get_info();
        if (fb && fb->is_active) {
            target = fb->back_buffer;
            target_w = static_cast<int>(fb->width);
            target_h = static_cast<int>(fb->height);
        }
    }
}

void gfx_draw_pixel(int x, int y, uint32_t color, uint32_t* target, int target_w, int target_h) {
    get_target_info(target, target_w, target_h);
    if (!target || x < 0 || y < 0 || x >= target_w || y >= target_h) {
        return;
    }

    uint8_t alpha = static_cast<uint8_t>((color >> 24) & 0xFF);
    size_t idx = static_cast<size_t>(y) * static_cast<size_t>(target_w) + static_cast<size_t>(x);

    if (alpha < 255) {
        target[idx] = gfx_alpha_blend(target[idx], color, alpha);
    } else {
        target[idx] = color;
    }
}

void gfx_draw_rect(int x, int y, int w, int h, uint32_t color, uint32_t* target, int target_w, int target_h) {
    get_target_info(target, target_w, target_h);
    if (!target || w <= 0 || h <= 0) return;

    int x1 = x;
    int y1 = y;
    int x2 = x + w;
    int y2 = y + h;

    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 > target_w) x2 = target_w;
    if (y2 > target_h) y2 = target_h;
    if (x1 >= x2 || y1 >= y2) return;

    uint8_t alpha = static_cast<uint8_t>((color >> 24) & 0xFF);

    for (int cy = y1; cy < y2; ++cy) {
        size_t row_offset = static_cast<size_t>(cy) * static_cast<size_t>(target_w);
        for (int cx = x1; cx < x2; ++cx) {
            size_t idx = row_offset + static_cast<size_t>(cx);
            if (alpha < 255) {
                target[idx] = gfx_alpha_blend(target[idx], color, alpha);
            } else {
                target[idx] = color;
            }
        }
    }
}

void gfx_draw_rect_outline(int x, int y, int w, int h, uint32_t color, int thickness, uint32_t* target, int target_w, int target_h) {
    if (thickness <= 0) return;
    gfx_draw_rect(x, y, w, thickness, color, target, target_w, target_h);
    gfx_draw_rect(x, y + h - thickness, w, thickness, color, target, target_w, target_h);
    gfx_draw_rect(x, y + thickness, thickness, h - (thickness * 2), color, target, target_w, target_h);
    gfx_draw_rect(x + w - thickness, y + thickness, thickness, h - (thickness * 2), color, target, target_w, target_h);
}

void gfx_draw_gradient_v(int x, int y, int w, int h, uint32_t top, uint32_t bottom, uint32_t* target, int target_w, int target_h) {
    get_target_info(target, target_w, target_h);
    if (!target || w <= 0 || h <= 0) return;

    int r1 = (top >> 16) & 0xFF;
    int g1 = (top >> 8) & 0xFF;
    int b1 = top & 0xFF;

    int r2 = (bottom >> 16) & 0xFF;
    int g2 = (bottom >> 8) & 0xFF;
    int b2 = bottom & 0xFF;

    for (int row = 0; row < h; ++row) {
        int r = r1 + ((r2 - r1) * row) / h;
        int g = g1 + ((g2 - g1) * row) / h;
        int b = b1 + ((b2 - b1) * row) / h;
        uint32_t row_color = 0xFF000000 | (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
        gfx_draw_rect(x, y + row, w, 1, row_color, target, target_w, target_h);
    }
}

void gfx_draw_gradient_h(int x, int y, int w, int h, uint32_t left, uint32_t right, uint32_t* target, int target_w, int target_h) {
    get_target_info(target, target_w, target_h);
    if (!target || w <= 0 || h <= 0) return;

    int r1 = (left >> 16) & 0xFF;
    int g1 = (left >> 8) & 0xFF;
    int b1 = left & 0xFF;

    int r2 = (right >> 16) & 0xFF;
    int g2 = (right >> 8) & 0xFF;
    int b2 = right & 0xFF;

    for (int col = 0; col < w; ++col) {
        int r = r1 + ((r2 - r1) * col) / w;
        int g = g1 + ((g2 - g1) * col) / w;
        int b = b1 + ((b2 - b1) * col) / w;
        uint32_t col_color = 0xFF000000 | (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
        gfx_draw_rect(x + col, y, 1, h, col_color, target, target_w, target_h);
    }
}

void gfx_draw_line(int x0, int y0, int x1, int y1, uint32_t color, uint32_t* target, int target_w, int target_h) {
    int dx = (x1 >= x0) ? (x1 - x0) : (x0 - x1);
    int dy = (y1 >= y0) ? (y1 - y0) : (y0 - y1);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (true) {
        gfx_draw_pixel(x0, y0, color, target, target_w, target_h);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void gfx_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg, bool transparent, uint32_t* target, int target_w, int target_h) {
    const uint8_t* glyph = font_get_glyph(static_cast<unsigned char>(c));

    for (int row = 0; row < FONT_HEIGHT; ++row) {
        uint8_t row_bits = glyph[row];
        for (int col = 0; col < FONT_WIDTH; ++col) {
            if (row_bits & (0x80 >> col)) {
                gfx_draw_pixel(x + col, y + row, fg, target, target_w, target_h);
            } else if (!transparent) {
                gfx_draw_pixel(x + col, y + row, bg, target, target_w, target_h);
            }
        }
    }
}

void gfx_draw_text(int x, int y, const char* text, uint32_t fg, uint32_t bg, bool transparent, uint32_t* target, int target_w, int target_h) {
    if (!text) return;
    int cur_x = x;
    int cur_y = y;

    while (*text) {
        char c = *text++;
        if (c == '\n') {
            cur_x = x;
            cur_y += FONT_HEIGHT;
            continue;
        }
        if (c == '\r') {
            cur_x = x;
            continue;
        }
        gfx_draw_char(cur_x, cur_y, c, fg, bg, transparent, target, target_w, target_h);
        cur_x += FONT_WIDTH;
    }
}

void gfx_draw_text_shadow(int x, int y, const char* text, uint32_t fg, uint32_t shadow, uint32_t* target, int target_w, int target_h) {
    gfx_draw_text(x + 1, y + 1, text, shadow, 0, true, target, target_w, target_h);
    gfx_draw_text(x, y, text, fg, 0, true, target, target_w, target_h);
}

}
