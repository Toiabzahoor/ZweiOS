#include "gui/wm.hpp"
#include "gui/gfx.hpp"
#include "drivers/vbe.hpp"
#include "drivers/mouse.hpp"
#include "drivers/keyboard.hpp"
#include "drivers/serial.hpp"
#include "drivers/vga.hpp"
#include "drivers/rtc.hpp"
#include "drivers/pit.hpp"
#include "arch/x86_64/io.hpp"
#include "mm/heap.hpp"
#include "mm/pmm.hpp"
#include "lib/string.hpp"
#include "lib/kprintf.hpp"
#include "shell/shell.hpp"

namespace gui {

static Window   g_windows[MAX_WINDOWS];
static size_t   g_window_count = 0;
static uint32_t g_next_window_id = 1;
static bool     g_wm_running = false;
static Window*  g_active_window = nullptr;

static bool     g_prev_mouse_left = false;
static int      g_prev_mouse_x = -1;
static int      g_prev_mouse_y = -1;
static bool     g_menu_open = false;
static uint64_t g_last_clock_ticks = 0;
static bool     g_need_redraw = true;

inline constexpr int CURSOR_W = 20;
inline constexpr int CURSOR_H = 20;
static uint32_t g_cursor_under[CURSOR_W * CURSOR_H];
static int      g_cursor_saved_x = -1;
static int      g_cursor_saved_y = -1;
static bool     g_cursor_has_saved = false;

inline constexpr size_t TERM_MAX_ROWS = 64;
inline constexpr size_t TERM_MAX_COLS = 160;

static char    g_term_lines[TERM_MAX_ROWS][TERM_MAX_COLS];
static size_t  g_term_cur_row = 0;
static size_t  g_term_cur_col = 0;
static char    g_term_cmd[TERM_MAX_COLS];
static size_t  g_term_cmd_len = 0;
static Window* g_term_window = nullptr;

static void system_reboot(void) {
    drivers::serial_puts("[SYS] Rebooting system...\r\n");
    uint8_t status = 0x02;
    while (status & 0x02) {
        status = arch::inb(0x64);
    }
    arch::outb(0x64, 0xFE);
    while (true) {
        asm volatile("cli; hlt");
    }
}

static void wm_term_scroll(void) {
    for (size_t r = 0; r < TERM_MAX_ROWS - 1; ++r) {
        lib::memcpy(g_term_lines[r], g_term_lines[r + 1], TERM_MAX_COLS);
    }
    lib::memset(g_term_lines[TERM_MAX_ROWS - 1], 0, TERM_MAX_COLS);
}

void wm_term_putc(char c) {
    if (c == '\r') {
        g_term_cur_col = 0;
        return;
    }
    if (c == '\n') {
        g_term_cur_col = 0;
        if (g_term_cur_row + 1 < TERM_MAX_ROWS) {
            g_term_cur_row++;
        } else {
            wm_term_scroll();
        }
        return;
    }
    if (c == '\b' || c == 0x7F) {
        if (g_term_cur_col > 0) {
            g_term_cur_col--;
            g_term_lines[g_term_cur_row][g_term_cur_col] = '\0';
        }
        return;
    }
    if (c >= 32 && c <= 126) {
        if (g_term_cur_col + 1 >= TERM_MAX_COLS) {
            g_term_cur_col = 0;
            if (g_term_cur_row + 1 < TERM_MAX_ROWS) {
                g_term_cur_row++;
            } else {
                wm_term_scroll();
            }
        }
        g_term_lines[g_term_cur_row][g_term_cur_col++] = c;
        g_term_lines[g_term_cur_row][g_term_cur_col] = '\0';
    }
}

void wm_term_puts(const char* str) {
    if (!str) return;
    for (size_t i = 0; str[i] != '\0'; ++i) {
        wm_term_putc(str[i]);
    }
}

static void wm_term_redraw_window(void) {
    if (!g_term_window || !g_term_window->buffer) return;
    int ww = g_term_window->width;
    int wh = g_term_window->height;

    gfx_draw_rect(0, 0, ww, wh, 0xFF0B0F19, g_term_window->buffer, ww, wh);

    int start_y = 10;
    int line_h = 18;
    int max_visible = (wh - 28) / line_h;
    if (max_visible < 2) max_visible = 2;

    int total_lines = static_cast<int>(g_term_cur_row) + 1;
    int start_row = 0;
    if (total_lines > max_visible) {
        start_row = total_lines - max_visible;
    }

    for (int r = start_row; r < static_cast<int>(g_term_cur_row); ++r) {
        if (r >= 0 && r < static_cast<int>(TERM_MAX_ROWS)) {
            if (g_term_lines[r][0] != '\0') {
                int draw_y = start_y + (r - start_row) * line_h;
                gfx_draw_text(12, draw_y, g_term_lines[r], COLOR_WHITE, 0, true, g_term_window->buffer, ww, wh);
            }
        }
    }

    char prompt_line[TERM_MAX_COLS + 16];
    lib::strcpy(prompt_line, "zwei> ");
    lib::strcat(prompt_line, g_term_cmd);
    size_t plen = lib::strlen(prompt_line);
    if (plen < sizeof(prompt_line) - 2) {
        prompt_line[plen] = '_';
        prompt_line[plen + 1] = '\0';
    }
    int prompt_y = start_y + (static_cast<int>(g_term_cur_row) - start_row) * line_h;
    if (prompt_y + 20 > wh) prompt_y = wh - 22;
    gfx_draw_text(12, prompt_y, prompt_line, COLOR_GREEN, 0, true, g_term_window->buffer, ww, wh);
}

void wm_init(void) {
    g_window_count = 0;
    g_next_window_id = 1;
    g_wm_running = false;
    g_active_window = nullptr;
    g_prev_mouse_left = false;
    g_prev_mouse_x = -1;
    g_prev_mouse_y = -1;
    g_menu_open = false;
    g_last_clock_ticks = 0;
    g_need_redraw = true;

    g_cursor_saved_x = -1;
    g_cursor_saved_y = -1;
    g_cursor_has_saved = false;

    g_term_cur_row = 0;
    g_term_cur_col = 0;
    g_term_cmd_len = 0;
    g_term_cmd[0] = '\0';
    g_term_window = nullptr;

    for (size_t r = 0; r < TERM_MAX_ROWS; ++r) {
        lib::memset(g_term_lines[r], 0, TERM_MAX_COLS);
    }

    for (size_t i = 0; i < MAX_WINDOWS; ++i) {
        g_windows[i].id = 0;
        g_windows[i].title[0] = '\0';
        g_windows[i].x = 0;
        g_windows[i].y = 0;
        g_windows[i].width = 0;
        g_windows[i].height = 0;
        g_windows[i].flags = 0;
        g_windows[i].buffer = nullptr;
        g_windows[i].is_active = false;
        g_windows[i].is_minimized = false;
        g_windows[i].is_dragging = false;
        g_windows[i].drag_offset_x = 0;
        g_windows[i].drag_offset_y = 0;
    }
}

void wm_open_app_system_monitor(void) {
    for (size_t i = 0; i < MAX_WINDOWS; ++i) {
        if (g_windows[i].id != 0 && lib::strcmp(g_windows[i].title, "ZweiOS System Monitor") == 0) {
            g_windows[i].is_minimized = false;
            wm_focus_window(g_windows[i].id);
            g_need_redraw = true;
            return;
        }
    }

    auto* fb = drivers::vbe_get_info();
    uint32_t screen_w = fb ? fb->width : 1920;
    uint32_t screen_h = fb ? fb->height : 1080;

    int w1_w = (screen_w >= 1920) ? 680 : 440;
    int w1_h = (screen_h >= 1080) ? 400 : 280;
    Window* w1 = wm_create_window("ZweiOS System Monitor", 60, 60, w1_w, w1_h);
    if (w1 && w1->buffer) {
        gfx_draw_rect(0, 0, w1->width, w1->height, COLOR_WINDOW_BG, w1->buffer, w1->width, w1->height);
        gfx_draw_text(16, 16, "ZweiOS Native Dual-OS Kernel", COLOR_CYAN, 0, true, w1->buffer, w1->width, w1->height);
        gfx_draw_text(16, 36, "Author: made by toiabzahoor", COLOR_WHITE, 0, true, w1->buffer, w1->width, w1->height);
        gfx_draw_text(16, 60, "Subsystem: Linux POSIX + Win32", COLOR_GREEN, 0, true, w1->buffer, w1->width, w1->height);
        char res_str[64];
        lib::strcpy(res_str, "Display  : ");
        lib::strcat(res_str, (screen_w >= 1920) ? "1920x1080 32-bit True Color" : "1024x768 32-bit True Color");
        gfx_draw_text(16, 80, res_str, COLOR_LIGHT_GRAY, 0, true, w1->buffer, w1->width, w1->height);
        gfx_draw_text(16, 100, "Storage  : FAT32 On-Disk Filesystem", COLOR_YELLOW, 0, true, w1->buffer, w1->width, w1->height);

        size_t total_f = 0, used_f = 0, free_f = 0;
        mm::pmm_get_stats(&total_f, &used_f, &free_f);
        char mem_str[64];
        lib::strcpy(mem_str, "RAM Total: ");
        lib::strcat(mem_str, "256 MB (65536 pages)");
        gfx_draw_text(16, 130, mem_str, COLOR_LIGHT_GRAY, 0, true, w1->buffer, w1->width, w1->height);
        gfx_draw_text(16, 160, "Preemptive Scheduler: 100 Hz PIT Preemption", COLOR_CYAN, 0, true, w1->buffer, w1->width, w1->height);
    }
    g_need_redraw = true;
}

void wm_open_app_win32(void) {
    for (size_t i = 0; i < MAX_WINDOWS; ++i) {
        if (g_windows[i].id != 0 && lib::strcmp(g_windows[i].title, "Win32 Application View") == 0) {
            g_windows[i].is_minimized = false;
            wm_focus_window(g_windows[i].id);
            g_need_redraw = true;
            return;
        }
    }

    auto* fb = drivers::vbe_get_info();
    uint32_t screen_w = fb ? fb->width : 1920;
    uint32_t screen_h = fb ? fb->height : 1080;

    int w2_x = (screen_w >= 1920) ? 780 : 520;
    int w2_w = (screen_w >= 1920) ? 720 : 440;
    int w2_h = (screen_h >= 1080) ? 440 : 300;
    Window* w2 = wm_create_window("Win32 Application View", w2_x, 60, w2_w, w2_h);
    if (w2 && w2->buffer) {
        gfx_draw_rect(0, 0, w2->width, w2->height, COLOR_TERMINAL_BG, w2->buffer, w2->width, w2->height);
        gfx_draw_text(16, 16, "[USER32 / GDI32 Desktop View]", COLOR_BLUE, 0, true, w2->buffer, w2->width, w2->height);
        gfx_draw_text(16, 40, "Running Microsoft x64 GUI Dialogs", COLOR_WHITE, 0, true, w2->buffer, w2->width, w2->height);
        gfx_draw_rect_outline(16, 70, w2->width - 32, w2->height - 90, COLOR_MID_GRAY, 1, w2->buffer, w2->width, w2->height);
        gfx_draw_rect(30, 90, w2->width - 60, 30, COLOR_TITLE_ACTIVE, w2->buffer, w2->width, w2->height);
        gfx_draw_text(40, 98, "MessageBoxA: Welcome to ZweiOS GUI!", COLOR_WHITE, 0, true, w2->buffer, w2->width, w2->height);
        gfx_draw_text(40, 140, "Dual execution engine active.", COLOR_GREEN, 0, true, w2->buffer, w2->width, w2->height);
        gfx_draw_text(40, 165, "Click & drag title bars to move.", COLOR_LIGHT_GRAY, 0, true, w2->buffer, w2->width, w2->height);
        gfx_draw_text(40, 190, "Hardware Mouse: Auto-synchronized.", COLOR_CYAN, 0, true, w2->buffer, w2->width, w2->height);
    }
    g_need_redraw = true;
}

void wm_open_app_terminal(void) {
    for (size_t i = 0; i < MAX_WINDOWS; ++i) {
        if (g_windows[i].id != 0 && lib::strcmp(g_windows[i].title, "ZweiOS Desktop Terminal") == 0) {
            g_windows[i].is_minimized = false;
            wm_focus_window(g_windows[i].id);
            g_term_window = &g_windows[i];
            wm_term_redraw_window();
            g_need_redraw = true;
            return;
        }
    }

    auto* fb = drivers::vbe_get_info();
    uint32_t screen_w = fb ? fb->width : 1920;
    uint32_t screen_h = fb ? fb->height : 1080;

    int term_w = (screen_w >= 1920) ? 800 : 640;
    int term_h = (screen_h >= 1080) ? 520 : 380;
    int term_x = (screen_w >= 1920) ? 80 : 30;
    int term_y = (screen_h >= 1080) ? 60 : 45;

    Window* w3 = wm_create_window("ZweiOS Desktop Terminal", term_x, term_y, term_w, term_h);
    if (w3 && w3->buffer) {
        g_term_window = w3;
        if (g_term_cur_row == 0 && g_term_lines[0][0] == '\0') {
            wm_term_puts("ZweiOS Universal Terminal [x86_64 Long Mode]");
            wm_term_putc('\n');
            wm_term_puts("Type 'help' for available commands.");
            wm_term_putc('\n');
        }
        wm_term_redraw_window();
    }
    g_need_redraw = true;
}

bool wm_start(void) {
    if (!drivers::vbe_is_available()) {
        return false;
    }

    uint32_t screen_w = 1920;
    uint32_t screen_h = 1080;
    if (!drivers::vbe_set_mode(screen_w, screen_h, 32)) {
        screen_w = 1024;
        screen_h = 768;
        if (!drivers::vbe_set_mode(screen_w, screen_h, 32)) {
            return false;
        }
    }

    drivers::mouse_init();
    drivers::mouse_set_bounds(static_cast<int>(screen_w), static_cast<int>(screen_h));

    g_wm_running = true;
    g_menu_open = false;
    g_need_redraw = true;

    wm_open_app_terminal();

    wm_render();
    return true;
}

Window* wm_create_window(const char* title, int x, int y, int width, int height, uint32_t flags) {
    if (g_window_count >= MAX_WINDOWS || width <= 0 || height <= 0) {
        return nullptr;
    }

    size_t slot = g_window_count;
    for (size_t i = 0; i < MAX_WINDOWS; ++i) {
        if (g_windows[i].id == 0) {
            slot = i;
            break;
        }
    }

    Window& w = g_windows[slot];
    w.id = g_next_window_id++;
    lib::strncpy(w.title, title ? title : "Window", sizeof(w.title));
    w.x = x;
    w.y = y;
    w.width = width;
    w.height = height;
    w.flags = flags;
    w.is_active = true;
    w.is_minimized = false;
    w.is_dragging = false;
    w.drag_offset_x = 0;
    w.drag_offset_y = 0;

    size_t buf_bytes = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
    w.buffer = reinterpret_cast<uint32_t*>(mm::kmalloc(buf_bytes));
    if (w.buffer) {
        lib::memset(w.buffer, 0, buf_bytes);
    }

    for (size_t i = 0; i < MAX_WINDOWS; ++i) {
        if (g_windows[i].id != 0 && g_windows[i].id != w.id) {
            g_windows[i].is_active = false;
        }
    }

    g_active_window = &w;
    g_window_count++;
    g_need_redraw = true;
    return &w;
}

void wm_destroy_window(uint32_t id) {
    for (size_t i = 0; i < MAX_WINDOWS; ++i) {
        if (g_windows[i].id == id) {
            if (g_term_window && g_term_window->id == id) {
                g_term_window = nullptr;
            }
            if (g_windows[i].buffer) {
                mm::kfree(g_windows[i].buffer);
                g_windows[i].buffer = nullptr;
            }
            g_windows[i].id = 0;
            g_windows[i].is_active = false;
            g_windows[i].is_minimized = false;
            g_windows[i].is_dragging = false;
            g_window_count--;
            if (g_active_window == &g_windows[i]) {
                g_active_window = nullptr;
                for (int j = static_cast<int>(MAX_WINDOWS) - 1; j >= 0; --j) {
                    if (g_windows[j].id != 0 && !g_windows[j].is_minimized) {
                        g_windows[j].is_active = true;
                        g_active_window = &g_windows[j];
                        break;
                    }
                }
            }
            g_need_redraw = true;
            return;
        }
    }
}

Window* wm_get_window(uint32_t id) {
    for (size_t i = 0; i < MAX_WINDOWS; ++i) {
        if (g_windows[i].id == id) {
            return &g_windows[i];
        }
    }
    return nullptr;
}

void wm_focus_window(uint32_t id) {
    Window* target = nullptr;
    for (size_t i = 0; i < MAX_WINDOWS; ++i) {
        if (g_windows[i].id == id) {
            g_windows[i].is_active = true;
            target = &g_windows[i];
            if (lib::strcmp(g_windows[i].title, "ZweiOS Desktop Terminal") == 0) {
                g_term_window = &g_windows[i];
            }
        } else if (g_windows[i].id != 0) {
            g_windows[i].is_active = false;
        }
    }
    g_active_window = target;
    g_need_redraw = true;
}

static void render_desktop_background(void) {
    auto* fb = drivers::vbe_get_info();
    if (!fb) return;

    gfx_draw_gradient_v(0, 0, static_cast<int>(fb->width), static_cast<int>(fb->height), COLOR_DESKTOP_TOP, COLOR_DESKTOP_BOTTOM);

    const char* watermark = "ZweiOS Universal Dual-OS Platform - made by toiabzahoor";
    int wm_len = static_cast<int>(lib::strlen(watermark));
    int wm_x = (static_cast<int>(fb->width) - (wm_len * 8)) / 2;
    int wm_y = static_cast<int>(fb->height) - 48;
    gfx_draw_text(wm_x, wm_y, watermark, 0xFF334155, 0, true);
}

static void render_taskbar(void) {
    auto* fb = drivers::vbe_get_info();
    if (!fb) return;

    int screen_w = static_cast<int>(fb->width);

    gfx_draw_gradient_v(0, 0, screen_w, 32, 0xFF1E293B, 0xFF0F172A);
    gfx_draw_rect(0, 31, screen_w, 1, 0xFF334155);

    uint32_t btn_bg = g_menu_open ? 0xFF2563EB : COLOR_TITLE_ACTIVE;
    gfx_draw_rect(8, 4, 90, 24, btn_bg);
    gfx_draw_rect_outline(8, 4, 90, 24, 0xFF60A5FA, 1);
    gfx_draw_text(16, 8, "ZweiOS", COLOR_WHITE, 0, true);

    drivers::rtc_time_t t = drivers::rtc_get_time();
    char clock_str[32];
    drivers::rtc_format_time(&t, clock_str, sizeof(clock_str));
    gfx_draw_text(screen_w - 90, 8, clock_str, COLOR_LIGHT_GRAY, 0, true);
}

static void render_start_menu(void) {
    if (!g_menu_open) return;

    int menu_x = 8;
    int menu_y = 34;
    int menu_w = 280;
    int menu_h = 250;

    gfx_draw_rect(menu_x + 6, menu_y + 6, menu_w, menu_h, 0x66000000);
    gfx_draw_gradient_v(menu_x, menu_y, menu_w, menu_h, 0xFF1E293B, 0xFF0F172A);
    gfx_draw_rect_outline(menu_x, menu_y, menu_w, menu_h, 0xFF475569, 1);

    gfx_draw_rect(menu_x + 4, menu_y + 4, menu_w - 8, 26, 0xFF0F172A);
    gfx_draw_text(menu_x + 12, menu_y + 8, "ZweiOS Panel", COLOR_CYAN, 0, true);

    gfx_draw_rect(menu_x + 6, menu_y + 34, menu_w - 12, 22, 0xFF1E293B);
    gfx_draw_text(menu_x + 12, menu_y + 36, "[ Apps ]", COLOR_YELLOW, 0, true);

    gfx_draw_rect(menu_x + 6, menu_y + 58, menu_w - 12, 26, 0xFF0F172A);
    gfx_draw_text(menu_x + 18, menu_y + 62, "> System Monitor", COLOR_WHITE, 0, true);

    gfx_draw_rect(menu_x + 6, menu_y + 86, menu_w - 12, 26, 0xFF0F172A);
    gfx_draw_text(menu_x + 18, menu_y + 90, "> Win32 App View", COLOR_WHITE, 0, true);

    gfx_draw_rect(menu_x + 6, menu_y + 114, menu_w - 12, 26, 0xFF0F172A);
    gfx_draw_text(menu_x + 18, menu_y + 118, "> Desktop Terminal", COLOR_WHITE, 0, true);

    gfx_draw_rect(menu_x + 10, menu_y + 144, menu_w - 20, 1, 0xFF334155);

    gfx_draw_rect(menu_x + 6, menu_y + 150, menu_w - 12, 26, 0xFF0F172A);
    gfx_draw_text(menu_x + 14, menu_y + 154, "- Close All Windows", COLOR_MID_GRAY, 0, true);

    gfx_draw_rect(menu_x + 6, menu_y + 178, menu_w - 12, 26, 0xFF0F172A);
    gfx_draw_text(menu_x + 14, menu_y + 182, "* Restart System", COLOR_RED, 0, true);
}

static void render_single_window(Window& w) {
    if (w.id == 0 || w.is_minimized || !w.buffer) return;

    int title_h = 28;
    int wx = w.x;
    int wy = w.y;
    int ww = w.width;
    int wh = w.height;

    gfx_draw_rect(wx + 4, wy + 4, ww, wh + title_h, 0x55000000);

    uint32_t title_color = w.is_active ? COLOR_TITLE_ACTIVE : COLOR_TITLE_INACTIVE;
    gfx_draw_gradient_v(wx, wy, ww, title_h, title_color, title_color + 0x111111);

    gfx_draw_text_shadow(wx + 8, wy + 6, w.title, COLOR_WHITE, 0xFF000000);

    int close_w = 26;
    int close_h = 22;
    int close_x = wx + ww - close_w - 4;
    int close_y = wy + 3;
    gfx_draw_rect(close_x, close_y, close_w, close_h, 0xFFEF4444);
    gfx_draw_rect_outline(close_x, close_y, close_w, close_h, 0xFFFCA5A5, 1);
    gfx_draw_text(close_x + 9, close_y + 3, "x", COLOR_WHITE, 0, true);

    int client_y = wy + title_h;
    auto* fb = drivers::vbe_get_info();
    if (!fb) return;

    for (int r = 0; r < wh; ++r) {
        int screen_row = client_y + r;
        if (screen_row < 0 || screen_row >= static_cast<int>(fb->height)) continue;

        for (int c = 0; c < ww; ++c) {
            int screen_col = wx + c;
            if (screen_col < 0 || screen_col >= static_cast<int>(fb->width)) continue;

            uint32_t px = w.buffer[r * ww + c];
            fb->back_buffer[screen_row * fb->width + screen_col] = px;
        }
    }

    gfx_draw_rect_outline(wx, wy, ww, wh + title_h, 0xFF475569, 1);
}

static void wm_update_cursor_fast(int new_x, int new_y) {
    auto* fb = drivers::vbe_get_info();
    if (!fb || !fb->front_buffer) return;

    if (g_cursor_has_saved && g_cursor_saved_x >= 0 && g_cursor_saved_y >= 0) {
        for (int r = 0; r < CURSOR_H; ++r) {
            int sy = g_cursor_saved_y + r;
            if (sy < 0 || sy >= static_cast<int>(fb->height)) continue;
            for (int c = 0; c < CURSOR_W; ++c) {
                int sx = g_cursor_saved_x + c;
                if (sx < 0 || sx >= static_cast<int>(fb->width)) continue;
                fb->front_buffer[sy * fb->width + sx] = g_cursor_under[r * CURSOR_W + c];
            }
        }
    }

    g_cursor_saved_x = new_x;
    g_cursor_saved_y = new_y;
    for (int r = 0; r < CURSOR_H; ++r) {
        int sy = new_y + r;
        if (sy < 0 || sy >= static_cast<int>(fb->height)) continue;
        for (int c = 0; c < CURSOR_W; ++c) {
            int sx = new_x + c;
            if (sx < 0 || sx >= static_cast<int>(fb->width)) continue;
            g_cursor_under[r * CURSOR_W + c] = fb->front_buffer[sy * fb->width + sx];
        }
    }
    g_cursor_has_saved = true;

    drivers::mouse_draw_cursor(fb->front_buffer, static_cast<int>(fb->width), static_cast<int>(fb->height));
}

void wm_render(void) {
    if (!g_wm_running) return;

    render_desktop_background();

    for (size_t i = 0; i < MAX_WINDOWS; ++i) {
        if (g_windows[i].id != 0 && &g_windows[i] != g_active_window) {
            render_single_window(g_windows[i]);
        }
    }

    if (g_active_window && g_active_window->id != 0) {
        render_single_window(*g_active_window);
    }

    render_taskbar();

    render_start_menu();

    auto* fb = drivers::vbe_get_info();
    if (!fb || !fb->front_buffer || !fb->back_buffer) return;

    lib::memcpy(fb->front_buffer, fb->back_buffer, fb->width * fb->height * 4);

    drivers::MouseState m = drivers::mouse_get_state();
    g_cursor_saved_x = m.x;
    g_cursor_saved_y = m.y;
    for (int r = 0; r < CURSOR_H; ++r) {
        int sy = m.y + r;
        if (sy < 0 || sy >= static_cast<int>(fb->height)) continue;
        for (int c = 0; c < CURSOR_W; ++c) {
            int sx = m.x + c;
            if (sx < 0 || sx >= static_cast<int>(fb->width)) continue;
            g_cursor_under[r * CURSOR_W + c] = fb->front_buffer[sy * fb->width + sx];
        }
    }
    g_cursor_has_saved = true;

    drivers::mouse_draw_cursor(fb->front_buffer, static_cast<int>(fb->width), static_cast<int>(fb->height));
    g_need_redraw = false;
}

void wm_update(void) {
    if (!g_wm_running) return;

    drivers::MouseState m = drivers::mouse_get_state();

    int cx = m.x;
    int cy = m.y;
    bool clicked = drivers::mouse_consume_click(&cx, &cy);
    if (!clicked && m.left_button && !g_prev_mouse_left) {
        clicked = true;
        cx = m.x;
        cy = m.y;
    }

    bool mouse_moved = (m.x != g_prev_mouse_x || m.y != g_prev_mouse_y);
    if (mouse_moved) {
        g_prev_mouse_x = m.x;
        g_prev_mouse_y = m.y;
    }

    uint64_t cur_ticks = drivers::timer_get_ticks();
    if (cur_ticks >= g_last_clock_ticks + 100) {
        g_last_clock_ticks = cur_ticks;
        g_need_redraw = true;
    }

    int menu_x = 8;
    int menu_y = 34;
    int menu_w = 280;
    int menu_h = 250;

    if (clicked) {
        g_need_redraw = true;

        if (g_menu_open) {
            if (cx >= menu_x && cx <= menu_x + menu_w && cy >= menu_y && cy <= menu_y + menu_h) {
                int item_sysmon_y   = menu_y + 56;
                int item_win32_y    = menu_y + 85;
                int item_term_y     = menu_y + 113;
                int item_closeall_y = menu_y + 145;
                int item_reboot_y   = menu_y + 177;

                if (cy >= item_sysmon_y && cy < item_win32_y) {
                    wm_open_app_system_monitor();
                } else if (cy >= item_win32_y && cy < item_term_y) {
                    wm_open_app_win32();
                } else if (cy >= item_term_y && cy < item_closeall_y) {
                    wm_open_app_terminal();
                } else if (cy >= item_closeall_y && cy < item_reboot_y) {
                    for (size_t i = 0; i < MAX_WINDOWS; ++i) {
                        if (g_windows[i].id != 0) {
                            wm_destroy_window(g_windows[i].id);
                        }
                    }
                } else if (cy >= item_reboot_y && cy <= menu_y + menu_h) {
                    system_reboot();
                }
                g_menu_open = false;
                g_prev_mouse_left = m.left_button;
                wm_render();
                return;
            } else if (cx < 0 || cx > 110 || cy < 0 || cy > 34) {
                g_menu_open = false;
            }
        }

        if (cx >= 0 && cx <= 110 && cy >= 0 && cy <= 34) {
            g_menu_open = !g_menu_open;
            g_prev_mouse_left = m.left_button;
            wm_render();
            return;
        }

        bool handled = false;
        if (g_active_window && g_active_window->id != 0 && !g_active_window->is_minimized) {
            Window& w = *g_active_window;
            int title_h = 28;
            int wx = w.x;
            int wy = w.y;
            int ww = w.width;
            int wh = w.height;

            if (cx >= wx + ww - 48 && cx <= wx + ww + 8 && cy >= wy && cy <= wy + 32) {
                wm_destroy_window(w.id);
                handled = true;
            } else if (cx >= wx && cx < wx + ww && cy >= wy && cy < wy + wh + title_h) {
                if (cy < wy + title_h && (w.flags & WINDOW_FLAG_MOVABLE)) {
                    w.is_dragging = true;
                    w.drag_offset_x = cx - w.x;
                    w.drag_offset_y = cy - w.y;
                }
                handled = true;
            }
        }

        if (!handled) {
            for (int i = static_cast<int>(MAX_WINDOWS) - 1; i >= 0; --i) {
                Window& w = g_windows[i];
                if (w.id == 0 || w.is_minimized) continue;
                if (&w == g_active_window) continue;

                int title_h = 28;
                int wx = w.x;
                int wy = w.y;
                int ww = w.width;
                int wh = w.height;

                if (cx >= wx + ww - 48 && cx <= wx + ww + 8 && cy >= wy && cy <= wy + 32) {
                    wm_destroy_window(w.id);
                    break;
                }

                if (cx >= wx && cx < wx + ww && cy >= wy && cy < wy + wh + title_h) {
                    wm_focus_window(w.id);
                    if (cy < wy + title_h && (w.flags & WINDOW_FLAG_MOVABLE)) {
                        w.is_dragging = true;
                        w.drag_offset_x = cx - w.x;
                        w.drag_offset_y = cy - w.y;
                    }
                    break;
                }
            }
        }
    } else if (m.left_button && g_prev_mouse_left) {
        if (g_active_window && g_active_window->is_dragging) {
            g_active_window->x = m.x - g_active_window->drag_offset_x;
            g_active_window->y = m.y - g_active_window->drag_offset_y;
            if (g_active_window->y < 32) g_active_window->y = 32;
            g_need_redraw = true;
        }
    } else if (!m.left_button && g_prev_mouse_left) {
        if (g_active_window) {
            g_active_window->is_dragging = false;
        }
    }

    g_prev_mouse_left = m.left_button;

    char kc = 0;
    if (drivers::keyboard_try_getchar(&kc)) {
        if (g_active_window == g_term_window && g_term_window != nullptr && g_term_window->id != 0) {
            if (kc == '\r' || kc == '\n') {
                char exec_line[TERM_MAX_COLS + 16];
                lib::strcpy(exec_line, "zwei> ");
                lib::strcat(exec_line, g_term_cmd);
                wm_term_puts(exec_line);
                wm_term_putc('\n');

                if (g_term_cmd_len > 0) {
                    if (lib::strcmp(g_term_cmd, "clear") == 0) {
                        for (size_t r = 0; r < TERM_MAX_ROWS; ++r) {
                            lib::memset(g_term_lines[r], 0, TERM_MAX_COLS);
                        }
                        g_term_cur_row = 0;
                        g_term_cur_col = 0;
                    } else {
                        lib::kprint_set_hook(wm_term_putc);
                        shell::shell_dispatch(g_term_cmd);
                        lib::kprint_set_hook(nullptr);
                    }
                }
                g_term_cmd_len = 0;
                g_term_cmd[0] = '\0';
                wm_term_redraw_window();
                g_need_redraw = true;
            } else if (kc == '\b' || kc == 0x7F) {
                if (g_term_cmd_len > 0) {
                    g_term_cmd_len--;
                    g_term_cmd[g_term_cmd_len] = '\0';
                    wm_term_redraw_window();
                    g_need_redraw = true;
                }
            } else if (kc == 27) {
                if (g_menu_open) {
                    g_menu_open = false;
                    g_need_redraw = true;
                } else {
                    g_term_cmd_len = 0;
                    g_term_cmd[0] = '\0';
                    wm_term_redraw_window();
                    g_need_redraw = true;
                }
            } else if (kc >= 32 && kc <= 126) {
                if (g_term_cmd_len + 1 < TERM_MAX_COLS - 10) {
                    g_term_cmd[g_term_cmd_len++] = kc;
                    g_term_cmd[g_term_cmd_len] = '\0';
                    wm_term_redraw_window();
                    g_need_redraw = true;
                }
            }
        } else if (kc == 27) {
            if (g_menu_open) {
                g_menu_open = false;
                g_need_redraw = true;
            }
        }
    }

    char sc = 0;
    while (drivers::serial_try_getc(&sc)) {
        shell::shell_feed_char(sc);
    }

    if (g_need_redraw) {
        wm_render();
    } else if (mouse_moved) {
        wm_update_cursor_fast(m.x, m.y);
    }
}

bool wm_is_running(void) {
    return g_wm_running;
}

void wm_stop(void) {
    g_wm_running = false;
}

void wm_toggle_menu(void) {
    g_menu_open = !g_menu_open;
    g_need_redraw = true;
}

bool wm_is_menu_open(void) {
    return g_menu_open;
}

void wm_run_loop(void) {
    if (!g_wm_running) return;

    drivers::serial_puts("[GUI] Desktop active.\r\n");
    asm volatile("sti");

    while (g_wm_running) {
        wm_update();
        asm volatile("sti; hlt");
    }

    drivers::vga_restore_text_mode();
    g_wm_running = false;
}

Window* wm_get_active_window(void) {
    return g_active_window;
}

size_t wm_get_window_count(void) {
    return g_window_count;
}

}
