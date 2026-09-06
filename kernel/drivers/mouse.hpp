#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

namespace drivers {

struct MouseState {
    int  x;
    int  y;
    bool left_button;
    bool right_button;
    bool middle_button;
    bool left_clicked;
    int  click_x;
    int  click_y;
};

void        mouse_init(void);
void        mouse_handle_irq(void);
void        mouse_poll(void);
bool        mouse_is_vmmouse(void);
MouseState  mouse_get_state(void);
bool        mouse_consume_click(int* out_x, int* out_y);
void        mouse_set_bounds(int width, int height);
void        mouse_draw_cursor(uint32_t* target, int target_w, int target_h);

}
