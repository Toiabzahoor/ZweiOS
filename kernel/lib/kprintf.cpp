

#include "lib/kprintf.hpp"
#include "drivers/vga.hpp"
#include "drivers/serial.hpp"

namespace lib {

static void (*g_kprint_hook)(char c) = nullptr;

void kprint_set_hook(void (*hook)(char c)) {
    g_kprint_hook = hook;
}

void kprint_char(char c) {
    drivers::serial_putc(c);
    if (g_kprint_hook) {
        g_kprint_hook(c);
    } else {
        drivers::vga_putc(c);
    }
}

void kprint_str(const char* str) {
    if (!str) {
        kprint_str("(null)");
        return;
    }
    for (size_t i = 0; str[i] != '\0'; ++i) {
        kprint_char(str[i]);
    }
}

void kprint_udec(uint64_t val) {
    if (val == 0) {
        kprint_char('0');
        return;
    }
    char buf[24];
    int pos = 0;
    while (val > 0) {
        buf[pos++] = static_cast<char>('0' + (val % 10));
        val /= 10;
    }
    for (int i = pos - 1; i >= 0; --i) {
        kprint_char(buf[i]);
    }
}

void kprint_dec(int64_t val) {
    if (val < 0) {
        kprint_char('-');
        val = -val;
    }
    kprint_udec(static_cast<uint64_t>(val));
}

void kprint_hex(uint64_t val, int width) {
    const char hex_digits[] = "0123456789ABCDEF";
    char buf[16];
    for (int i = 15; i >= 0; --i) {
        buf[i] = hex_digits[val & 0xF];
        val >>= 4;
    }
    int start = 0;
    if (width > 0 && width <= 16) {
        start = 16 - width;
    } else {
        while (start < 15 && buf[start] == '0') {
            start++;
        }
    }
    for (int i = start; i < 16; ++i) {
        kprint_char(buf[i]);
    }
}

void kprint_ptr(const void* ptr) {
    kprint_str("0x");
    kprint_hex(reinterpret_cast<uint64_t>(ptr), 16);
}

void kvprintf(const char* fmt, va_list args) {
    if (!fmt) return;

    for (size_t i = 0; fmt[i] != '\0'; ++i) {
        if (fmt[i] != '%') {
            kprint_char(fmt[i]);
            continue;
        }

        i++;
        if (fmt[i] == '\0') break;


        int width = 0;
        if (fmt[i] >= '0' && fmt[i] <= '9') {
            width = fmt[i] - '0';
            i++;
        }

        switch (fmt[i]) {
            case 'c': {
                char c = static_cast<char>(va_arg(args, int));
                kprint_char(c);
                break;
            }
            case 's': {
                const char* s = va_arg(args, const char*);
                kprint_str(s);
                break;
            }
            case 'd':
            case 'i': {
                int64_t val = va_arg(args, int);
                kprint_dec(val);
                break;
            }
            case 'u': {
                uint64_t val = va_arg(args, unsigned int);
                kprint_udec(val);
                break;
            }
            case 'x':
            case 'X': {
                uint64_t val = va_arg(args, unsigned long long);
                kprint_hex(val, width);
                break;
            }
            case 'p': {
                const void* p = va_arg(args, const void*);
                kprint_ptr(p);
                break;
            }
            case '%': {
                kprint_char('%');
                break;
            }
            default: {
                kprint_char('%');
                kprint_char(fmt[i]);
                break;
            }
        }
    }
}

void kprintf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    kvprintf(fmt, args);
    va_end(args);
}

}
