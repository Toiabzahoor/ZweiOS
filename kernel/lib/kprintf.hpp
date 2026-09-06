

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

namespace lib {

void kprint_char(char c);
void kprint_str(const char* str);
void kprint_dec(int64_t val);
void kprint_udec(uint64_t val);
void kprint_hex(uint64_t val, int width = 0);
void kprint_ptr(const void* ptr);

void kvprintf(const char* fmt, va_list args);
void kprintf(const char* fmt, ...);
void kprint_set_hook(void (*hook)(char c));

}

using lib::kprint_char;
using lib::kprint_str;
using lib::kprint_dec;
using lib::kprint_udec;
using lib::kprint_hex;
using lib::kprint_ptr;
using lib::kvprintf;
using lib::kprintf;
