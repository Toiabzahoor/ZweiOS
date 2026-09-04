/* ==============================================================================
 * ZweiOS - Bare-Metal x86_64 Operating System
 * Component: Freestanding String and Memory Manipulation Implementation
 * ============================================================================== */

#include "lib/string.hpp"

namespace lib {

extern "C" {

void* memset(void* dest, int val, size_t count) {
    uint8_t* ptr = static_cast<uint8_t*>(dest);
    uint8_t byte = static_cast<uint8_t>(val);
    for (size_t i = 0; i < count; ++i) {
        ptr[i] = byte;
    }
    return dest;
}

void* memcpy(void* dest, const void* src, size_t count) {
    uint8_t* d = static_cast<uint8_t*>(dest);
    const uint8_t* s = static_cast<const uint8_t*>(src);
    for (size_t i = 0; i < count; ++i) {
        d[i] = s[i];
    }
    return dest;
}

void* memmove(void* dest, const void* src, size_t count) {
    uint8_t* d = static_cast<uint8_t*>(dest);
    const uint8_t* s = static_cast<const uint8_t*>(src);
    if (d < s) {
        for (size_t i = 0; i < count; ++i) {
            d[i] = s[i];
        }
    } else if (d > s) {
        for (size_t i = count; i > 0; --i) {
            d[i - 1] = s[i - 1];
        }
    }
    return dest;
}

int memcmp(const void* s1, const void* s2, size_t count) {
    const uint8_t* p1 = static_cast<const uint8_t*>(s1);
    const uint8_t* p2 = static_cast<const uint8_t*>(s2);
    for (size_t i = 0; i < count; ++i) {
        if (p1[i] != p2[i]) {
            return p1[i] - p2[i];
        }
    }
    return 0;
}

size_t strlen(const char* str) {
    if (!str) return 0;
    size_t len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

int strcmp(const char* s1, const char* s2) {
    if (!s1 && !s2) return 0;
    if (!s1) return -1;
    if (!s2) return 1;
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return static_cast<int>(static_cast<unsigned char>(*s1)) - static_cast<int>(static_cast<unsigned char>(*s2));
}

int strncmp(const char* s1, const char* s2, size_t n) {
    if (n == 0) return 0;
    if (!s1 && !s2) return 0;
    if (!s1) return -1;
    if (!s2) return 1;
    while (n > 1 && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    return static_cast<int>(static_cast<unsigned char>(*s1)) - static_cast<int>(static_cast<unsigned char>(*s2));
}

char* strcpy(char* dest, const char* src) {
    if (!dest || !src) return dest;
    char* ret = dest;
    while ((*dest++ = *src++) != '\0') {}
    return ret;
}

char* strncpy(char* dest, const char* src, size_t n) {
    if (!dest || !src) return dest;
    char* ret = dest;
    size_t i = 0;
    for (; i < n && src[i] != '\0'; ++i) {
        dest[i] = src[i];
    }
    for (; i < n; ++i) {
        dest[i] = '\0';
    }
    return ret;
}

char* strcat(char* dest, const char* src) {
    if (!dest || !src) return dest;
    char* ret = dest;
    while (*dest != '\0') {
        dest++;
    }
    while ((*dest++ = *src++) != '\0') {}
    return ret;
}

} // extern "C"

bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

int string_to_int(const char* str) {
    if (!str) return 0;
    while (is_space(*str)) str++;
    int sign = 1;
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }
    int result = 0;
    while (is_digit(*str)) {
        result = result * 10 + (*str - '0');
        str++;
    }
    return sign * result;
}

} // namespace lib
