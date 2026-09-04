#pragma once
#include <stdint.h>

namespace arch {

// 8-bit Port Output
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

// 8-bit Port Input
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// 16-bit Port Output
static inline void outw(uint16_t port, uint16_t val) {
    asm volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

// 16-bit Port Input
static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    asm volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// 32-bit Port Output
static inline void outl(uint16_t port, uint32_t val) {
    asm volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

// 32-bit Port Input
static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    asm volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// I/O Delay (~1 microsecond via POST checkpoint port 0x80)
static inline void io_wait(void) {
    asm volatile ("outb %%al, $0x80" : : "a"((uint8_t)0));
}

// QEMU ISA Debug-Exit Constants & Primitives
// Writing val to port 0xF4 causes QEMU to exit with host code (val << 1) | 1
inline constexpr uint16_t QEMU_DEBUG_EXIT_PORT = 0xF4;
inline constexpr uint8_t  QEMU_EXIT_SUCCESS     = 0x10; // QEMU exit code 33 (0x21)
inline constexpr uint8_t  QEMU_EXIT_FAILURE     = 0x11; // QEMU exit code 35 (0x23)
inline constexpr uint8_t  QEMU_EXIT_PANIC       = 0x12; // QEMU exit code 37 (0x25)

static inline void qemu_debug_exit(uint8_t code) {
    outb(QEMU_DEBUG_EXIT_PORT, code);
}

// ==============================================================================
// Model-Specific Registers (MSRs)
// ==============================================================================
inline constexpr uint32_t IA32_EFER          = 0xC0000080;
inline constexpr uint32_t IA32_STAR          = 0xC0000081;
inline constexpr uint32_t IA32_LSTAR         = 0xC0000082;
inline constexpr uint32_t IA32_CSTAR         = 0xC0000083;
inline constexpr uint32_t IA32_FMASK         = 0xC0000084;
inline constexpr uint32_t IA32_FS_BASE       = 0xC0000100;
inline constexpr uint32_t IA32_GS_BASE       = 0xC0000101;
inline constexpr uint32_t IA32_KERNEL_GS_BASE= 0xC0000102;

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t low, high;
    asm volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return (static_cast<uint64_t>(high) << 32) | low;
}

static inline void wrmsr(uint32_t msr, uint64_t val) {
    uint32_t low = static_cast<uint32_t>(val & 0xFFFFFFFF);
    uint32_t high = static_cast<uint32_t>(val >> 32);
    asm volatile ("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

} // namespace arch

// Global aliases for kernel-wide convenience
using arch::outb;
using arch::inb;
using arch::outw;
using arch::inw;
using arch::outl;
using arch::inl;
using arch::io_wait;
using arch::qemu_debug_exit;
using arch::rdmsr;
using arch::wrmsr;

