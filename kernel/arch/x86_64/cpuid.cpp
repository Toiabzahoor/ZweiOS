/* ==============================================================================
 * ZweiOS - Bare-Metal x86_64 Operating System
 * Architecture: x86_64 Long Mode
 * Component: CPUID Instruction Interface & Feature Decoder Implementation
 * ============================================================================== */

#include "arch/x86_64/cpuid.hpp"
#include "drivers/serial.hpp"
#include "lib/kprintf.hpp"

namespace arch {

static cpu_info_t cached_cpu_info;
static bool cpu_info_cached = false;

void cpuid_detect(cpu_info_t* info) {
    if (!info) return;

    for (size_t i = 0; i < sizeof(cpu_info_t); ++i) {
        reinterpret_cast<uint8_t*>(info)[i] = 0;
    }

    uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;

    // 1. Leaf 0: Vendor String & Maximum Standard Leaf
    cpuid(0, 0, &eax, &ebx, &ecx, &edx);
    info->max_leaf = eax;

    // EBX, EDX, ECX contain 12-byte vendor string
    *reinterpret_cast<uint32_t*>(&info->vendor[0]) = ebx;
    *reinterpret_cast<uint32_t*>(&info->vendor[4]) = edx;
    *reinterpret_cast<uint32_t*>(&info->vendor[8]) = ecx;
    info->vendor[12] = '\0';

    // 2. Leaf 1: Family, Model, Stepping & Features
    if (info->max_leaf >= 1) {
        cpuid(1, 0, &eax, &ebx, &ecx, &edx);
        info->stepping = eax & 0x0F;
        uint32_t base_model = (eax >> 4) & 0x0F;
        uint32_t base_family = (eax >> 8) & 0x0F;
        uint32_t ext_model = (eax >> 16) & 0x0F;
        uint32_t ext_family = (eax >> 20) & 0xFF;

        info->family = (base_family == 0x0F) ? (base_family + ext_family) : base_family;
        info->model  = (base_family == 0x06 || base_family == 0x0F) ? (base_model | (ext_model << 4)) : base_model;

        info->features_edx1 = edx;
        info->features_ecx1 = ecx;
    }

    // 3. Leaf 7: Extended Feature Flags
    if (info->max_leaf >= 7) {
        cpuid(7, 0, &eax, &ebx, &ecx, &edx);
        info->features_ebx7 = ebx;
    }

    // 4. Extended Leaves: Brand String (0x80000000..0x80000004)
    cpuid(0x80000000, 0, &eax, &ebx, &ecx, &edx);
    info->max_ext_leaf = eax;

    if (info->max_ext_leaf >= 0x80000004) {
        uint32_t* brand_words = reinterpret_cast<uint32_t*>(info->brand);
        cpuid(0x80000002, 0, &brand_words[0], &brand_words[1], &brand_words[2], &brand_words[3]);
        cpuid(0x80000003, 0, &brand_words[4], &brand_words[5], &brand_words[6], &brand_words[7]);
        cpuid(0x80000004, 0, &brand_words[8], &brand_words[9], &brand_words[10], &brand_words[11]);
        info->brand[48] = '\0';
    } else {
        const char* default_brand = "x86_64 Compatible Processor";
        for (size_t i = 0; default_brand[i] != '\0' && i < 48; ++i) {
            info->brand[i] = default_brand[i];
        }
    }
}

static void print_uint32(uint32_t val) {
    lib::kprint_udec(val);
}

void cpuid_print_report() {
    if (!cpu_info_cached) {
        cpuid_detect(&cached_cpu_info);
        cpu_info_cached = true;
    }

    lib::kprint_str("=== CPU Hardware Information ===\r\n");
    lib::kprint_str("  Vendor String : ");
    lib::kprint_str(cached_cpu_info.vendor);
    lib::kprint_str("\r\n");

    lib::kprint_str("  Brand String  : ");
    lib::kprint_str(cached_cpu_info.brand);
    lib::kprint_str("\r\n");

    lib::kprint_str("  Family/Model  : Family ");
    print_uint32(cached_cpu_info.family);
    lib::kprint_str(", Model ");
    print_uint32(cached_cpu_info.model);
    lib::kprint_str(", Stepping ");
    print_uint32(cached_cpu_info.stepping);
    lib::kprint_str("\r\n");

    lib::kprint_str("  Features      : ");

    // Standard EDX features (Leaf 1)
    if (cached_cpu_info.features_edx1 & (1 << 0))  lib::kprint_str("FPU ");
    if (cached_cpu_info.features_edx1 & (1 << 4))  lib::kprint_str("TSC ");
    if (cached_cpu_info.features_edx1 & (1 << 5))  lib::kprint_str("MSR ");
    if (cached_cpu_info.features_edx1 & (1 << 6))  lib::kprint_str("PAE ");
    if (cached_cpu_info.features_edx1 & (1 << 9))  lib::kprint_str("APIC ");
    if (cached_cpu_info.features_edx1 & (1 << 13)) lib::kprint_str("PGE ");
    if (cached_cpu_info.features_edx1 & (1 << 15)) lib::kprint_str("CMOV ");
    if (cached_cpu_info.features_edx1 & (1 << 23)) lib::kprint_str("MMX ");
    if (cached_cpu_info.features_edx1 & (1 << 24)) lib::kprint_str("FXSR ");
    if (cached_cpu_info.features_edx1 & (1 << 25)) lib::kprint_str("SSE ");
    if (cached_cpu_info.features_edx1 & (1 << 26)) lib::kprint_str("SSE2 ");

    // Standard ECX features (Leaf 1)
    if (cached_cpu_info.features_ecx1 & (1 << 0))  lib::kprint_str("SSE3 ");
    if (cached_cpu_info.features_ecx1 & (1 << 9))  lib::kprint_str("SSSE3 ");
    if (cached_cpu_info.features_ecx1 & (1 << 19)) lib::kprint_str("SSE4.1 ");
    if (cached_cpu_info.features_ecx1 & (1 << 20)) lib::kprint_str("SSE4.2 ");
    if (cached_cpu_info.features_ecx1 & (1 << 28)) lib::kprint_str("AVX ");
    if (cached_cpu_info.features_ecx1 & (1 << 30)) lib::kprint_str("RDRAND ");

    // Extended EBX features (Leaf 7)
    if (cached_cpu_info.features_ebx7 & (1 << 5))  lib::kprint_str("AVX2 ");

    lib::kprint_str("\r\n");
}

} // namespace arch
