#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

namespace toolchain {

struct CompileResult {
    bool   success;
    size_t output_size;
    int    exit_code;
};

void toolchain_init();
CompileResult compile_c(const char* source_code, size_t source_len, uint8_t* out_bin, size_t max_bin_size, bool is_windows = false);
bool compile_file(const char* source_path, const char* output_path, bool is_windows = false);
int build_make(const char* makefile_path, const char* target = nullptr);

int cmd_gcc(int argc, char* argv[]);
int cmd_gpp(int argc, char* argv[]);
int cmd_tcc(int argc, char* argv[]);
int cmd_make(int argc, char* argv[]);

}
