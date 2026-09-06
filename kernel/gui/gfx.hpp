#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

namespace gui {

inline constexpr uint32_t COLOR_BLACK          = 0xFF000000;
inline constexpr uint32_t COLOR_WHITE          = 0xFFFFFFFF;
inline constexpr uint32_t COLOR_RED            = 0xFFFF4444;
inline constexpr uint32_t COLOR_GREEN          = 0xFF44CC44;
inline constexpr uint32_t COLOR_BLUE           = 0xFF3388FF;
inline constexpr uint32_t COLOR_CYAN           = 0xFF00DDDD;
inline constexpr uint32_t COLOR_MAGENTA        = 0xFFDD00DD;
inline constexpr uint32_t COLOR_YELLOW         = 0xFFFFFF33;
inline constexpr uint32_t COLOR_DARK_GRAY      = 0xFF1E1E2E;
inline constexpr uint32_t COLOR_MID_GRAY       = 0xFF313244;
inline constexpr uint32_t COLOR_LIGHT_GRAY     = 0xFFBAC2DE;
inline constexpr uint32_t COLOR_DESKTOP_TOP    = 0xFF0F172A;
inline constexpr uint32_t COLOR_DESKTOP_BOTTOM = 0xFF020617;
inline constexpr uint32_t COLOR_TASKBAR        = 0xFF181825;
inline constexpr uint32_t COLOR_WINDOW_BG      = 0xFF181825;
inline constexpr uint32_t COLOR_TITLE_ACTIVE   = 0xFF2563EB;
inline constexpr uint32_t COLOR_TITLE_INACTIVE = 0xFF334155;
inline constexpr uint32_t COLOR_TERMINAL_BG    = 0xFF0B0F19;

inline constexpr uint32_t make_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    return (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
}

void     gfx_init(void);
uint32_t gfx_alpha_blend(uint32_t bg, uint32_t fg, uint8_t alpha);
void     gfx_draw_pixel(int x, int y, uint32_t color, uint32_t* target = nullptr, int target_w = 0, int target_h = 0);
void     gfx_draw_rect(int x, int y, int w, int h, uint32_t color, uint32_t* target = nullptr, int target_w = 0, int target_h = 0);
void     gfx_draw_rect_outline(int x, int y, int w, int h, uint32_t color, int thickness = 1, uint32_t* target = nullptr, int target_w = 0, int target_h = 0);
void     gfx_draw_gradient_v(int x, int y, int w, int h, uint32_t top, uint32_t bottom, uint32_t* target = nullptr, int target_w = 0, int target_h = 0);
void     gfx_draw_gradient_h(int x, int y, int w, int h, uint32_t left, uint32_t right, uint32_t* target = nullptr, int target_w = 0, int target_h = 0);
void     gfx_draw_line(int x0, int y0, int x1, int y1, uint32_t color, uint32_t* target = nullptr, int target_w = 0, int target_h = 0);
void     gfx_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg = 0, bool transparent = true, uint32_t* target = nullptr, int target_w = 0, int target_h = 0);
void     gfx_draw_text(int x, int y, const char* text, uint32_t fg, uint32_t bg = 0, bool transparent = true, uint32_t* target = nullptr, int target_w = 0, int target_h = 0);
void     gfx_draw_text_shadow(int x, int y, const char* text, uint32_t fg, uint32_t shadow = COLOR_BLACK, uint32_t* target = nullptr, int target_w = 0, int target_h = 0);

}
