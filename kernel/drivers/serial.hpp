#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define COM1_PORT 0x3F8

namespace drivers {

void serial_init();
void serial_putc(char c);
void serial_puts(const char* str);
void serial_put_hex(uint64_t val);
void serial_put_hex64(uint64_t val);
void serial_put_dec(uint64_t val);
bool serial_try_getc(char* out);

} // namespace drivers

// Global aliases for kernel-wide convenience
using drivers::serial_init;
using drivers::serial_putc;
using drivers::serial_puts;
using drivers::serial_put_hex;
using drivers::serial_put_hex64;
using drivers::serial_put_dec;
using drivers::serial_try_getc;
