

#include "drivers/keyboard.hpp"
#include "arch/x86_64/io.hpp"
#include "arch/x86_64/pic.hpp"
#include "arch/x86_64/isr.hpp"
#include "drivers/serial.hpp"

namespace drivers {


static char ring_buffer[KEYBOARD_BUFFER_SIZE];
static volatile size_t ring_head = 0;
static volatile size_t ring_tail = 0;

static void ring_push(char c) {
    size_t next = (ring_head + 1) % KEYBOARD_BUFFER_SIZE;
    if (next != ring_tail) {
        ring_buffer[ring_head] = c;
        ring_head = next;
    }
}



static bool shift_pressed = false;
static bool ctrl_pressed  = false;
static bool alt_pressed   = false;
static bool caps_locked   = false;
static bool extended_e0   = false;


static const char scancode_set1_normal[128] = {
    0,   27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   '-', 0,   0,   0,   '+', 0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};


static const char scancode_set1_shifted[128] = {
    0,   27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   '-', 0,   0,   0,   '+', 0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

static inline bool interrupts_enabled() {
    uint64_t rflags;
    asm volatile("pushfq; popq %0" : "=r"(rflags));
    return (rflags & (1 << 9)) != 0;
}

static bool ring_pop(char* out) {
    if (ring_head == ring_tail) return false;

    uint64_t rflags;
    asm volatile("pushfq; popq %0; cli" : "=r"(rflags));

    if (ring_head == ring_tail) {
        if (rflags & (1 << 9)) asm volatile("sti");
        return false;
    }

    if (out) {
        *out = ring_buffer[ring_tail];
    }
    ring_tail = (ring_tail + 1) % KEYBOARD_BUFFER_SIZE;

    if (rflags & (1 << 9)) asm volatile("sti");
    return true;
}

static void process_scancode(uint8_t scancode) {

    if (scancode == 0xE0) {
        extended_e0 = true;
        return;
    }


    if (extended_e0) {
        extended_e0 = false;

        return;
    }


    if (scancode & 0x80) {
        uint8_t released = scancode & 0x7F;
        if (released == 0x2A || released == 0x36) {
            shift_pressed = false;
        } else if (released == 0x1D) {
            ctrl_pressed = false;
        } else if (released == 0x38) {
            alt_pressed = false;
        }
        return;
    }


    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = true;
        return;
    }

    if (scancode == 0x1D) {
        ctrl_pressed = true;
        return;
    }

    if (scancode == 0x38) {
        alt_pressed = true;
        return;
    }

    if (scancode == 0x3A) {
        caps_locked = !caps_locked;
        return;
    }


    if (ctrl_pressed) {
        if (scancode == 0x2E) {
            ring_push('\x03');
        } else if (scancode == 0x26) {
            ring_push('\x0c');
        }
        return;
    }


    char ascii = 0;
    bool use_upper = (shift_pressed ^ caps_locked);

    if (scancode < 128) {
        char normal_ch = scancode_set1_normal[scancode];
        if (normal_ch >= 'a' && normal_ch <= 'z') {
            ascii = use_upper ? scancode_set1_shifted[scancode] : normal_ch;
        } else {
            ascii = shift_pressed ? scancode_set1_shifted[scancode] : normal_ch;
        }
    }

    if (ascii != 0) {
        ring_push(ascii);
    }
}

void keyboard_poll() {
    while ((arch::inb(0x64) & 0x21) == 0x01) {
        uint8_t scancode = arch::inb(0x60);
        process_scancode(scancode);
    }
}

void keyboard_irq_handler(arch::cpu_registers_t* ) {
    while ((arch::inb(0x64) & 0x21) == 0x01) {
        uint8_t scancode = arch::inb(0x60);
        process_scancode(scancode);
    }
    arch::pic_send_eoi(1);
}

bool keyboard_has_char() {
    if (!interrupts_enabled()) keyboard_poll();
    return ring_head != ring_tail;
}

char keyboard_getchar() {
    char c = 0;
    while (!ring_pop(&c)) {
        if (!interrupts_enabled()) keyboard_poll();
        asm volatile("pause");
    }
    return c;
}

bool keyboard_try_getchar(char* out) {
    if (!interrupts_enabled()) keyboard_poll();
    return ring_pop(out);
}

bool sys_try_getc(char* out) {

    if (ring_pop(out)) {
        return true;
    }

    if (!interrupts_enabled()) {
        keyboard_poll();
        if (ring_pop(out)) {
            return true;
        }
    }

    return serial_try_getc(out);
}

void keyboard_init() {

    while (arch::inb(0x64) & 0x01) {
        arch::inb(0x60);
    }


    arch::outb(0x64, 0xAE);


    arch::isr_register_handler(33, keyboard_irq_handler);


    arch::pic_clear_mask(1);

    drivers::serial_puts("[PS/2] 8042 Keyboard Controller initialized; Ring buffer ready\r\n");
}

}
