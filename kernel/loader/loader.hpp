/* ==============================================================================
 * ZweiOS - Bare-Metal x86_64 Operating System
 * Component: Universal Dual Binary Loader (ELF64 & PE32+)
 * ============================================================================== */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "loader/binary_info.hpp"
#include "loader/elf.hpp"
#include "loader/pe.hpp"

namespace loader {

BinaryFormat loader_detect_format(const uint8_t* data, size_t size);
bool loader_inspect(const uint8_t* data, size_t size, BinaryInfo* out_info);
void loader_print_info(const BinaryInfo* info);

// Memory mapping and execution
bool loader_load(const uint8_t* data, size_t size, const BinaryInfo* info);
int64_t loader_execute(const BinaryInfo* info);
int64_t loader_execute_path(const char* path);

// Embedded sample test binaries for shell verification and testing
const uint8_t* loader_get_sample_elf(size_t* out_size);
const uint8_t* loader_get_sample_pe(size_t* out_size);
const uint8_t* loader_get_sample_win_sysinfo(size_t* out_size);
const uint8_t* loader_get_sample_win_calc(size_t* out_size);
const uint8_t* loader_get_sample_win_life(size_t* out_size);

} // namespace loader

extern "C" int64_t loader_invoke_entry(uint64_t entry_point, uint64_t stack_top, bool is_windows);
