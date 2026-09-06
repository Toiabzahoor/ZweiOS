

#pragma once

#include <stdint.h>
#include <stddef.h>

namespace lib {

extern "C" {
    void* memset(void* dest, int val, size_t count);
    void* memcpy(void* dest, const void* src, size_t count);
    void* memmove(void* dest, const void* src, size_t count);
    int memcmp(const void* s1, const void* s2, size_t count);

    size_t strlen(const char* str);
    int strcmp(const char* s1, const char* s2);
    int strncmp(const char* s1, const char* s2, size_t n);
    char* strcpy(char* dest, const char* src);
    char* strncpy(char* dest, const char* src, size_t n);
    char* strcat(char* dest, const char* src);
}


bool is_digit(char c);
bool is_space(char c);
int string_to_int(const char* str);

}

using lib::memset;
using lib::memcpy;
using lib::memmove;
using lib::memcmp;
using lib::strlen;
using lib::strcmp;
using lib::strncmp;
using lib::strcpy;
using lib::strncpy;
using lib::strcat;
using lib::is_digit;
using lib::is_space;
using lib::string_to_int;
