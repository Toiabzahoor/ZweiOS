#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

namespace gui {

inline constexpr size_t MAX_WINDOWS = 16;

inline constexpr uint32_t WINDOW_FLAG_MOVABLE    = 0x01;
inline constexpr uint32_t WINDOW_FLAG_RESIZABLE  = 0x02;
inline constexpr uint32_t WINDOW_FLAG_HAS_TITLE  = 0x04;
inline constexpr uint32_t WINDOW_FLAG_POPUP      = 0x08;

struct Window {
    uint32_t  id;
    char      title[64];
    int       x;
    int       y;
    int       width;
    int       height;
    uint32_t  flags;
    uint32_t* buffer;
    bool      is_active;
    bool      is_minimized;
    bool      is_dragging;
    int       drag_offset_x;
    int       drag_offset_y;
};

void    wm_init(void);
bool    wm_start(void);
Window* wm_create_window(const char* title, int x, int y, int width, int height, uint32_t flags = (WINDOW_FLAG_MOVABLE | WINDOW_FLAG_HAS_TITLE));
void    wm_destroy_window(uint32_t id);
Window* wm_get_window(uint32_t id);
void    wm_focus_window(uint32_t id);
void    wm_render(void);
void    wm_update(void);
void    wm_run_loop(void);
bool    wm_is_running(void);
void    wm_stop(void);

Window* wm_get_active_window(void);
size_t  wm_get_window_count(void);

void    wm_open_app_system_monitor(void);
void    wm_open_app_win32(void);
void    wm_open_app_terminal(void);
void    wm_term_putc(char c);
void    wm_term_puts(const char* str);
void    wm_toggle_menu(void);
bool    wm_is_menu_open(void);

}
