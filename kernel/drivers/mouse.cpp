#include "drivers/mouse.hpp"
#include "drivers/serial.hpp"
#include "lib/kprintf.hpp"
#include "arch/x86_64/io.hpp"
#include "arch/x86_64/pic.hpp"
#include "gui/gfx.hpp"

namespace drivers {

static MouseState g_mouse_state = {
    .x = 960,
    .y = 540,
    .left_button = false,
    .right_button = false,
    .middle_button = false,
    .left_clicked = false,
    .click_x = 960,
    .click_y = 540,
};

static int     g_screen_width  = 1920;
static int     g_screen_height = 1080;
static uint8_t g_mouse_cycle   = 0;
static uint8_t g_mouse_packet[3];
static bool    g_vmmouse_active = false;

static inline void vmmouse_call(uint32_t cmd, uint32_t in_ebx, uint32_t* out_eax, uint32_t* out_ebx, uint32_t* out_ecx, uint32_t* out_edx) {
    uint32_t eax = 0x564D5868;
    uint32_t ebx = in_ebx;
    uint32_t ecx = cmd;
    uint32_t edx = 0x5658;
    asm volatile(
        "inl %%dx, %%eax"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(eax), "b"(ebx), "c"(ecx), "d"(edx)
        : "memory"
    );
    if (out_eax) *out_eax = eax;
    if (out_ebx) *out_ebx = ebx;
    if (out_ecx) *out_ecx = ecx;
    if (out_edx) *out_edx = edx;
}

static bool vmmouse_init_device(void) {
    uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;
    vmmouse_call(10, 0, &eax, &ebx, &ecx, &edx);
    if (ebx != 0x564D5868) {
        return false;
    }

    vmmouse_call(41, 0x45414552, &eax, &ebx, &ecx, &edx);
    vmmouse_call(39, 1, &eax, &ebx, &ecx, &edx);
    if (eax != 0x3442554A) {
        return false;
    }

    vmmouse_call(41, 0x53424152, &eax, &ebx, &ecx, &edx);
    return true;
}

static void vmmouse_poll(void) {
    uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;
    vmmouse_call(40, 0, &eax, &ebx, &ecx, &edx);
    int count = static_cast<int>(eax & 0xFFFF);
    while (count >= 4) {
        vmmouse_call(39, 4, &eax, &ebx, &ecx, &edx);
        uint32_t flags = eax;
        uint32_t raw_x = ebx;
        uint32_t raw_y = ecx;

        int new_x = static_cast<int>((static_cast<uint64_t>(raw_x) * static_cast<uint64_t>(g_screen_width)) / 65535ULL);
        int new_y = static_cast<int>((static_cast<uint64_t>(raw_y) * static_cast<uint64_t>(g_screen_height)) / 65535ULL);

        if (new_x < 0) new_x = 0;
        if (new_y < 0) new_y = 0;
        if (new_x >= g_screen_width) new_x = g_screen_width - 1;
        if (new_y >= g_screen_height) new_y = g_screen_height - 1;

        g_mouse_state.x = new_x;
        g_mouse_state.y = new_y;

        bool left_pressed = (flags & 0x20) != 0;
        if (left_pressed && !g_mouse_state.left_button) {
            g_mouse_state.left_clicked = true;
            g_mouse_state.click_x = new_x;
            g_mouse_state.click_y = new_y;
        }

        g_mouse_state.left_button = left_pressed;
        g_mouse_state.right_button = (flags & 0x10) != 0;
        g_mouse_state.middle_button = (flags & 0x08) != 0;

        count -= 4;
    }
}

static void mouse_wait_write() {
    int timeout = 100000;
    while (timeout--) {
        if ((inb(0x64) & 0x02) == 0) return;
        io_wait();
    }
}

static void mouse_wait_read() {
    int timeout = 100000;
    while (timeout--) {
        if (inb(0x64) & 0x01) return;
        io_wait();
    }
}

static void mouse_write(uint8_t write_val) {
    mouse_wait_write();
    outb(0x64, 0xD4);
    mouse_wait_write();
    outb(0x60, write_val);
}

static uint8_t mouse_read() {
    mouse_wait_read();
    return inb(0x60);
}

void mouse_init(void) {
    g_mouse_state.x = g_screen_width / 2;
    g_mouse_state.y = g_screen_height / 2;
    g_mouse_state.left_button = false;
    g_mouse_state.right_button = false;
    g_mouse_state.middle_button = false;
    g_mouse_state.left_clicked = false;
    g_mouse_state.click_x = g_mouse_state.x;
    g_mouse_state.click_y = g_mouse_state.y;
    g_mouse_cycle = 0;
    g_vmmouse_active = false;

    while (inb(0x64) & 0x01) {
        inb(0x60);
    }

    mouse_wait_write();
    outb(0x64, 0xA8);

    mouse_wait_write();
    outb(0x64, 0x20);
    uint8_t status = mouse_read();
    status |= 0x02;
    status &= ~0x20;

    mouse_wait_write();
    outb(0x64, 0x60);
    mouse_wait_write();
    outb(0x60, status);

    mouse_write(0xF6);
    mouse_read();

    mouse_write(0xF4);
    mouse_read();

    while (inb(0x64) & 0x01) {
        inb(0x60);
    }

    if (vmmouse_init_device()) {
        g_vmmouse_active = true;
        drivers::serial_puts("[MOUSE] VMMouse absolute pointer active.\r\n");
    } else {
        drivers::serial_puts("[MOUSE] Standard PS/2 mouse active.\r\n");
    }

    arch::pic_clear_mask(2);
    arch::pic_clear_mask(12);
}

bool mouse_is_vmmouse(void) {
    return g_vmmouse_active;
}

void mouse_set_bounds(int width, int height) {
    g_screen_width = width;
    g_screen_height = height;
    if (g_mouse_state.x >= width) g_mouse_state.x = width - 1;
    if (g_mouse_state.y >= height) g_mouse_state.y = height - 1;
}

static void process_mouse_byte(uint8_t data) {
    if (g_mouse_cycle == 0) {
        if (!(data & 0x08)) {
            return;
        }
        g_mouse_packet[0] = data;
        g_mouse_cycle = 1;
    } else if (g_mouse_cycle == 1) {
        g_mouse_packet[1] = data;
        g_mouse_cycle = 2;
    } else if (g_mouse_cycle == 2) {
        g_mouse_packet[2] = data;
        g_mouse_cycle = 0;

        uint8_t flags = g_mouse_packet[0];
        int dx = static_cast<int>(g_mouse_packet[1]);
        int dy = static_cast<int>(g_mouse_packet[2]);

        if (flags & 0x10) dx |= ~0xFF;
        if (flags & 0x20) dy |= ~0xFF;

        if (flags & 0x40) {
            dx = (flags & 0x10) ? -127 : 127;
        }
        if (flags & 0x80) {
            dy = (flags & 0x20) ? -127 : 127;
        }

        g_mouse_state.x += dx;
        g_mouse_state.y -= dy;

        if (g_mouse_state.x < 0) g_mouse_state.x = 0;
        if (g_mouse_state.y < 0) g_mouse_state.y = 0;
        if (g_mouse_state.x >= g_screen_width) g_mouse_state.x = g_screen_width - 1;
        if (g_mouse_state.y >= g_screen_height) g_mouse_state.y = g_screen_height - 1;

        bool left_pressed = (flags & 0x01) != 0;
        if (left_pressed && !g_mouse_state.left_button) {
            g_mouse_state.left_clicked = true;
            g_mouse_state.click_x = g_mouse_state.x;
            g_mouse_state.click_y = g_mouse_state.y;
        }

        g_mouse_state.left_button = left_pressed;
        g_mouse_state.right_button = (flags & 0x02) != 0;
        g_mouse_state.middle_button = (flags & 0x04) != 0;
    }
}

void mouse_poll(void) {
    uint64_t rflags;
    asm volatile("pushfq; popq %0; cli" : "=r"(rflags));

    if (g_vmmouse_active) {
        vmmouse_poll();
    }

    while (inb(0x64) & 0x01) {
        uint8_t status = inb(0x64);
        if (status & 0x20) {
            uint8_t data = inb(0x60);
            if (!g_vmmouse_active) {
                process_mouse_byte(data);
            }
        } else {
            break;
        }
    }

    if (rflags & (1 << 9)) asm volatile("sti");
}

void mouse_handle_irq(void) {
    mouse_poll();
}

MouseState mouse_get_state(void) {
    mouse_poll();
    return g_mouse_state;
}

bool mouse_consume_click(int* out_x, int* out_y) {
    uint64_t rflags;
    asm volatile("pushfq; popq %0; cli" : "=r"(rflags));
    bool clicked = g_mouse_state.left_clicked;
    if (clicked) {
        if (out_x) *out_x = g_mouse_state.click_x;
        if (out_y) *out_y = g_mouse_state.click_y;
        g_mouse_state.left_clicked = false;
    }
    if (rflags & (1 << 9)) asm volatile("sti");
    return clicked;
}

static const char* g_cursor_bitmap[] = {
    "XX                  ",
    "X.X                 ",
    "X..X                ",
    "X...X               ",
    "X....X              ",
    "X.....X             ",
    "X......X            ",
    "X.......X           ",
    "X........X          ",
    "X.........X         ",
    "X..........X        ",
    "X...........X       ",
    "X......XXXXXX       ",
    "X...X..X            ",
    "X..X X..X           ",
    "X.X  X..X           ",
    "XX    X..X          ",
    "X     X..X          ",
    "       XX           ",
    nullptr
};

void mouse_draw_cursor(uint32_t* target, int target_w, int target_h) {
    int mx = g_mouse_state.x;
    int my = g_mouse_state.y;

    for (int row = 0; g_cursor_bitmap[row] != nullptr; ++row) {
        const char* line = g_cursor_bitmap[row];
        for (int col = 0; line[col] != '\0'; ++col) {
            char c = line[col];
            if (c == 'X') {
                gui::gfx_draw_pixel(mx + col, my + row, 0xFF000000, target, target_w, target_h);
            } else if (c == '.') {
                gui::gfx_draw_pixel(mx + col, my + row, 0xFFFFFFFF, target, target_w, target_h);
            }
        }
    }
}

}
