

#pragma once

#include <stdint.h>
#include <stddef.h>

namespace arch {

struct cpu_info_t {
    char vendor[13];
    char brand[49];
    uint32_t family;
    uint32_t model;
    uint32_t stepping;
    uint32_t max_leaf;
    uint32_t max_ext_leaf;
    uint32_t features_edx1;
    uint32_t features_ecx1;
    uint32_t features_ebx7;
};


static inline void cpuid(uint32_t leaf, uint32_t subleaf, uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx) {
    asm volatile("cpuid"
                 : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
                 : "a"(leaf), "c"(subleaf));
}


void cpuid_detect(cpu_info_t* info);


void cpuid_print_report();

}

using arch::cpu_info_t;
using arch::cpuid;
using arch::cpuid_detect;
using arch::cpuid_print_report;
