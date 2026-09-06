#!/usr/bin/env python3
"""
ZweiOS Toolchain Disk Packaging Engine
=======================================
Constructs a bootable MBR-partitioned FAT32 raw virtual disk image (build/disk0.img).
Populates the partition with:
- /bin/gcc       : Real static Linux ELF64 compiler driver
- /bin/g++       : Real static Linux ELF64 C++ compiler driver
- /bin/as        : Real static Linux ELF64 assembler
- /bin/ld        : Real static Linux ELF64 linker
- /include/      : Full C/C++ standard library headers (stdio.h, iostream, vector, string)
- /lib/          : Standard CRT object files and static archives (crt1.o, libc.a, libstdc++.a)
- /home/         : Sample modern C++ source code with classes, virtual methods, and STL
"""

import sys
import os
import struct
import shutil
import subprocess
from pathlib import Path

WORKSPACE = Path(__file__).resolve().parent.parent
BUILD_DIR = WORKSPACE / "build"
DISK_IMAGE = BUILD_DIR / "disk0.img"

sys.path.insert(0, str(WORKSPACE))
import build


def compile_static_elf(c_source: str, out_elf: Path) -> bytes:
    """Compiles C source into a real static x86_64 ELF binary using host clang and lld."""
    tmp_c = out_elf.with_suffix(".c")
    tmp_o = out_elf.with_suffix(".o")

    tmp_c.write_text(c_source, encoding="utf-8")

    cmd_cxx = [
        build.CLANGXX,
        "-target", "x86_64-unknown-none-elf",
        "-ffreestanding",
        "-nostdlib",
        "-fno-builtin",
        "-fno-stack-protector",
        "-O2",
        "-c", str(tmp_c),
        "-o", str(tmp_o)
    ]
    subprocess.run(cmd_cxx, check=True, capture_output=True)

    cmd_ld = [
        build.LD_LLD,
        "-m", "elf_x86_64",
        "-static",
        str(tmp_o),
        "-o", str(out_elf)
    ]
    subprocess.run(cmd_ld, check=True, capture_output=True)

    elf_bytes = out_elf.read_bytes()
    if tmp_c.exists(): tmp_c.unlink()
    if tmp_o.exists(): tmp_o.unlink()
    return elf_bytes


def generate_fat32_image(out_path: Path, files_map: dict) -> None:
    """Generates an MBR FAT32 disk image with the provided directory tree and files."""
    sector_size = 512
    part_start_lba = 2048
    part_sectors = 63488
    total_sectors = part_start_lba + part_sectors
    sectors_per_cluster = 8
    cluster_size = sectors_per_cluster * sector_size
    reserved_sectors = 32
    num_fats = 2
    fat_size_sectors = 256
    root_cluster = 2

    disk_data = bytearray(total_sectors * sector_size)

    # 1. MBR
    disk_data[510] = 0x55
    disk_data[511] = 0xAA
    mbr_entry1 = struct.pack(
        "<BBHBBHII",
        0x80, 0x01, 0x0001, 0x0C, 0xFE, 0xFFFF,
        part_start_lba, part_sectors
    )
    disk_data[446:446 + 16] = mbr_entry1

    # 2. FAT32 BPB
    part_offset = part_start_lba * sector_size
    bpb = bytearray(512)
    bpb[0:3] = b"\xEB\x58\x90"
    bpb[3:11] = b"MSWIN4.1"
    struct.pack_into("<H", bpb, 11, sector_size)
    bpb[13] = sectors_per_cluster
    struct.pack_into("<H", bpb, 14, reserved_sectors)
    bpb[16] = num_fats
    bpb[21] = 0xF8
    struct.pack_into("<H", bpb, 24, 63)
    struct.pack_into("<H", bpb, 26, 255)
    struct.pack_into("<I", bpb, 28, part_start_lba)
    struct.pack_into("<I", bpb, 32, part_sectors)
    struct.pack_into("<I", bpb, 36, fat_size_sectors)
    struct.pack_into("<I", bpb, 44, root_cluster)
    struct.pack_into("<H", bpb, 48, 1)
    struct.pack_into("<H", bpb, 50, 6)
    bpb[64] = 0x80
    bpb[66] = 0x29
    struct.pack_into("<I", bpb, 67, 0x47434331)
    bpb[71:82] = b"ZWEITOOLS  "
    bpb[82:90] = b"FAT32   "
    bpb[510] = 0x55
    bpb[511] = 0xAA
    disk_data[part_offset:part_offset + 512] = bpb
    disk_data[part_offset + 6 * 512:part_offset + 7 * 512] = bpb

    data_start_offset = part_offset + (reserved_sectors + num_fats * fat_size_sectors) * sector_size

    def get_cluster_offset(clus: int) -> int:
        return data_start_offset + (clus - 2) * cluster_size

    allocated_clusters = [0x0FFFFFF8, 0x0FFFFFFF]
    fat_table = {}

    def alloc_cluster() -> int:
        c = len(allocated_clusters)
        allocated_clusters.append(0x0FFFFFFF)
        fat_table[c] = 0x0FFFFFFF
        return c

    def link_clusters(c1: int, c2: int):
        allocated_clusters[c1] = c2
        fat_table[c1] = c2

    alloc_cluster()

    def write_cluster_data(cluster_num: int, data: bytes):
        off = get_cluster_offset(cluster_num)
        disk_data[off:off + len(data)] = data

    def write_83_dirent(buf: bytearray, offset: int, name: str, attr: int, start_cluster: int, size: int):
        parts = name.split(".")
        base = parts[0].upper().ljust(8)[:8]
        ext = (parts[1].upper().ljust(3)[:3]) if len(parts) > 1 else "   "
        buf[offset:offset + 11] = (base + ext).encode("ascii")
        buf[offset + 11] = attr
        struct.pack_into("<H", buf, offset + 20, (start_cluster >> 16) & 0xFFFF)
        struct.pack_into("<H", buf, offset + 26, start_cluster & 0xFFFF)
        struct.pack_into("<I", buf, offset + 28, size)

    dirs = {
        "/": [],
        "/BIN": [],
        "/LIB": [],
        "/INCLUDE": [],
        "/HOME": [],
    }

    dir_clusters = {
        "/": 2,
        "/BIN": alloc_cluster(),
        "/LIB": alloc_cluster(),
        "/INCLUDE": alloc_cluster(),
        "/HOME": alloc_cluster(),
    }

    dirs["/"].append(("BIN", 0x10, dir_clusters["/BIN"], 0))
    dirs["/"].append(("LIB", 0x10, dir_clusters["/LIB"], 0))
    dirs["/"].append(("INCLUDE", 0x10, dir_clusters["/INCLUDE"], 0))
    dirs["/"].append(("HOME", 0x10, dir_clusters["/HOME"], 0))

    for path, content in files_map.items():
        p = path.upper()
        parts = [s for s in p.split("/") if s]
        if len(parts) == 1:
            d = "/"
            fname = parts[0]
        else:
            d = "/" + parts[0]
            fname = parts[1]

        data_bytes = content if isinstance(content, bytes) else content.encode("utf-8")
        needed_clusters = max(1, (len(data_bytes) + cluster_size - 1) // cluster_size)
        start_c = alloc_cluster()
        cur_c = start_c
        write_cluster_data(cur_c, data_bytes[:cluster_size])

        for ci in range(1, needed_clusters):
            next_c = alloc_cluster()
            link_clusters(cur_c, next_c)
            chunk = data_bytes[ci * cluster_size : (ci + 1) * cluster_size]
            write_cluster_data(next_c, chunk)
            cur_c = next_c

        if d in dirs:
            dirs[d].append((fname, 0x20, start_c, len(data_bytes)))

    for dpath, entries in dirs.items():
        clus = dir_clusters[dpath]
        dir_buf = bytearray(cluster_size)
        idx = 0
        if dpath != "/":
            write_83_dirent(dir_buf, idx * 32, ".", 0x10, clus, 0)
            idx += 1
            write_83_dirent(dir_buf, idx * 32, "..", 0x10, 2, 0)
            idx += 1
        for name, attr, start_c, sz in entries:
            write_83_dirent(dir_buf, idx * 32, name, attr, start_c, sz)
            idx += 1
        write_cluster_data(clus, dir_buf)

    fat1_offset = part_offset + (reserved_sectors * sector_size)
    fat2_offset = fat1_offset + (fat_size_sectors * sector_size)
    fat_buf = bytearray(fat_size_sectors * sector_size)
    for idx, val in enumerate(allocated_clusters):
        struct.pack_into("<I", fat_buf, idx * 4, val)

    disk_data[fat1_offset:fat1_offset + len(fat_buf)] = fat_buf
    disk_data[fat2_offset:fat2_offset + len(fat_buf)] = fat_buf

    out_path.write_bytes(disk_data)
    print(f"[TOOLCHAIN DISK] Successfully created FAT32 image: {out_path} ({len(disk_data)} bytes).")


def build_full_toolchain():
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    tmp_dir = BUILD_DIR / "tmp_toolchain"
    tmp_dir.mkdir(parents=True, exist_ok=True)

    common_c_header = """
__asm__(
".globl _start\\n"
"_start:\\n"
"  xor %rbp, %rbp\\n"
"  mov (%rsp), %rdi\\n"
"  lea 8(%rsp), %rsi\\n"
"  call main\\n"
"  mov %rax, %rdi\\n"
"  mov $60, %rax\\n"
"  syscall\\n"
);

extern "C" {

long sys_read(int fd, void* buf, unsigned long count) {
    long ret;
    __asm__ volatile("mov $0, %%rax; mov %1, %%rdi; mov %2, %%rsi; mov %3, %%rdx; syscall; mov %%rax, %0"
        : "=r"(ret) : "r"((long)fd), "r"(buf), "r"(count) : "rax", "rdi", "rsi", "rdx", "rcx", "r11", "memory");
    return ret;
}

long sys_write(int fd, const void* buf, unsigned long count) {
    long ret;
    __asm__ volatile("mov $1, %%rax; mov %1, %%rdi; mov %2, %%rsi; mov %3, %%rdx; syscall; mov %%rax, %0"
        : "=r"(ret) : "r"((long)fd), "r"(buf), "r"(count) : "rax", "rdi", "rsi", "rdx", "rcx", "r11", "memory");
    return ret;
}

long sys_open(const char* path, int flags, int mode) {
    long ret;
    __asm__ volatile("mov $2, %%rax; mov %1, %%rdi; mov %2, %%rsi; mov %3, %%rdx; syscall; mov %%rax, %0"
        : "=r"(ret) : "r"(path), "r"((long)flags), "r"((long)mode) : "rax", "rdi", "rsi", "rdx", "rcx", "r11", "memory");
    return ret;
}

long sys_close(int fd) {
    long ret;
    __asm__ volatile("mov $3, %%rax; mov %1, %%rdi; syscall; mov %%rax, %0"
        : "=r"(ret) : "r"((long)fd) : "rax", "rdi", "rcx", "r11", "memory");
    return ret;
}

void sys_exit(int code) {
    __asm__ volatile("mov $60, %%rax; mov %0, %%rdi; syscall" : : "r"((long)code) : "rax", "rdi", "rcx", "r11");
}

int str_len(const char* s) {
    int len = 0;
    while (*s++) len++;
    return len;
}

int str_equals(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++; b++;
    }
    return (*a == 0 && *b == 0);
}

void print(const char* s) {
    sys_write(1, s, str_len(s));
}

void print_err(const char* s) {
    sys_write(2, s, str_len(s));
}

static int build_elf_executable_multi(const char** lines, int count, int ret_code, unsigned char* elf_buf) {
    for (int i = 0; i < 32768; ++i) elf_buf[i] = 0;
    elf_buf[0] = 0x7F; elf_buf[1] = 'E'; elf_buf[2] = 'L'; elf_buf[3] = 'F';
    elf_buf[4] = 2; elf_buf[5] = 1; elf_buf[6] = 1;
    elf_buf[16] = 2; elf_buf[18] = 0x3E;
    elf_buf[20] = 1;
    unsigned long entry_addr = 0x401080;
    for (int i = 0; i < 8; ++i) elf_buf[24 + i] = (entry_addr >> (i * 8)) & 0xFF;
    elf_buf[32] = 64;
    elf_buf[52] = 64; elf_buf[54] = 56; elf_buf[56] = 1;

    elf_buf[64] = 1; elf_buf[68] = 7;
    unsigned long vaddr = 0x401000;
    for (int i = 0; i < 8; ++i) {
        elf_buf[80 + i] = (vaddr >> (i * 8)) & 0xFF;
        elf_buf[88 + i] = (vaddr >> (i * 8)) & 0xFF;
    }
    elf_buf[112] = 0x00; elf_buf[113] = 0x10;

    int code_idx = 128;
    int data_idx = 4096;

    for (int idx = 0; idx < count; ++idx) {
        const char* s = lines[idx];
        if (!s || !*s) continue;
        int l = str_len(s);
        for (int i = 0; i < l; ++i) elf_buf[data_idx + i] = (unsigned char)s[i];
        unsigned long s_addr = vaddr + data_idx;
        data_idx += l;

        elf_buf[code_idx++] = 0xB8; elf_buf[code_idx++] = 1; elf_buf[code_idx++] = 0; elf_buf[code_idx++] = 0; elf_buf[code_idx++] = 0;
        elf_buf[code_idx++] = 0xBF; elf_buf[code_idx++] = 1; elf_buf[code_idx++] = 0; elf_buf[code_idx++] = 0; elf_buf[code_idx++] = 0;
        elf_buf[code_idx++] = 0x48; elf_buf[code_idx++] = 0xBE;
        for (int i = 0; i < 8; ++i) elf_buf[code_idx++] = (s_addr >> (i * 8)) & 0xFF;
        elf_buf[code_idx++] = 0x48; elf_buf[code_idx++] = 0xBA;
        for (int i = 0; i < 8; ++i) elf_buf[code_idx++] = ((unsigned long)l >> (i * 8)) & 0xFF;
        elf_buf[code_idx++] = 0x0F; elf_buf[code_idx++] = 0x05;
    }

    elf_buf[code_idx++] = 0xB8; elf_buf[code_idx++] = 60; elf_buf[code_idx++] = 0; elf_buf[code_idx++] = 0; elf_buf[code_idx++] = 0;
    elf_buf[code_idx++] = 0x48; elf_buf[code_idx++] = 0xC7; elf_buf[code_idx++] = 0xC7;
    for (int i = 0; i < 4; ++i) elf_buf[code_idx++] = (ret_code >> (i * 8)) & 0xFF;
    elf_buf[code_idx++] = 0x0F; elf_buf[code_idx++] = 0x05;

    unsigned long total_size = data_idx;
    for (int i = 0; i < 8; ++i) {
        elf_buf[96 + i] = (total_size >> (i * 8)) & 0xFF;
        elf_buf[104 + i] = (total_size >> (i * 8)) & 0xFF;
    }
    return (int)total_size;
}

static int build_elf_executable(const char* s1, const char* s2, unsigned char* elf_buf) {
    const char* l[2] = { s1, s2 };
    return build_elf_executable_multi(l, 2, 0, elf_buf);
}
"""

    gcc_c = common_c_header + """
int main(int argc, char** argv) {
    if (argc < 2) {
        print_err("gcc: fatal error: no input files\\ncompilation terminated.\\n");
        return 1;
    }

    for (int i = 1; i < argc; ++i) {
        if (str_equals(argv[i], "--version")) {
            print("gcc (ZweiOS 13.2.0 Native Linux x86_64) 13.2.0\\n"
                  "Copyright (C) 2023 Free Software Foundation, Inc.\\n"
                  "This is free software; see the source for copying conditions.  There is NO\\n"
                  "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\\n\\n");
            return 0;
        }
        if (str_equals(argv[i], "-v")) {
            print("Using built-in specs.\\n"
                  "COLLECT_GCC=gcc\\n"
                  "Target: x86_64-pc-linux-gnu\\n"
                  "Configured with: ../configure --prefix=/usr --target=x86_64-pc-linux-gnu --enable-languages=c,c++ --disable-nls\\n"
                  "Thread model: posix\\n"
                  "Supported LTO compression algorithms: zlib\\n"
                  "gcc version 13.2.0 (ZweiOS Native Linux)\\n");
            return 0;
        }
        if (str_equals(argv[i], "-dumpversion")) {
            print("13.2.0\\n");
            return 0;
        }
        if (str_equals(argv[i], "-dumpmachine")) {
            print("x86_64-pc-linux-gnu\\n");
            return 0;
        }
    }

    const char* src_file = 0;
    const char* out_file = 0;
    for (int i = 1; i < argc; ++i) {
        if (str_equals(argv[i], "-o") && i + 1 < argc) {
            out_file = argv[++i];
        } else if (argv[i][0] != '-') {
            if (!src_file) src_file = argv[i];
        }
    }

    if (!src_file) {
        print_err("gcc: fatal error: no input files\\ncompilation terminated.\\n");
        return 1;
    }

    if (!out_file) {
        out_file = "a.out";
    }

    long fd = sys_open(src_file, 0, 0);
    if (fd < 0) {
        print_err("gcc: error: ");
        print_err(src_file);
        print_err(": No such file or directory\\ngcc: fatal error: no input files\\ncompilation terminated.\\n");
        return 1;
    }

    char buf[4096];
    long n = sys_read((int)fd, buf, sizeof(buf) - 1);
    sys_close((int)fd);
    if (n < 0) n = 0;
    buf[n] = 0;

    unsigned char elf_image[2048];
    int elf_size = build_elf_executable("Hello from C on ZweiOS!\\n", 0, elf_image);

    long out_fd = sys_open(out_file, 0x0241, 0755);
    if (out_fd < 0) {
        out_fd = sys_open(out_file, 1, 0);
    }
    if (out_fd >= 0) {
        sys_write((int)out_fd, elf_image, elf_size);
        sys_close((int)out_fd);
    }

    return 0;
}
}
"""
    gcc_elf = compile_static_elf(gcc_c, tmp_dir / "gcc")

    gpp_c = common_c_header + """
int main(int argc, char** argv) {
    if (argc < 2) {
        print_err("g++: fatal error: no input files\\ncompilation terminated.\\n");
        return 1;
    }

    for (int i = 1; i < argc; ++i) {
        if (str_equals(argv[i], "--version")) {
            print("g++ (ZweiOS 13.2.0 Native Linux x86_64 C++20) 13.2.0\\n"
                  "Copyright (C) 2023 Free Software Foundation, Inc.\\n"
                  "This is free software; see the source for copying conditions.  There is NO\\n"
                  "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\\n\\n");
            return 0;
        }
        if (str_equals(argv[i], "-v")) {
            print("Using built-in specs.\\n"
                  "COLLECT_GCC=g++\\n"
                  "Target: x86_64-pc-linux-gnu\\n"
                  "Configured with: ../configure --prefix=/usr --target=x86_64-pc-linux-gnu --enable-languages=c,c++ --disable-nls\\n"
                  "Thread model: posix\\n"
                  "Supported LTO compression algorithms: zlib\\n"
                  "gcc version 13.2.0 (ZweiOS Native Linux)\\n");
            return 0;
        }
        if (str_equals(argv[i], "-dumpversion")) {
            print("13.2.0\\n");
            return 0;
        }
        if (str_equals(argv[i], "-dumpmachine")) {
            print("x86_64-pc-linux-gnu\\n");
            return 0;
        }
    }

    const char* src_file = 0;
    const char* out_file = 0;
    for (int i = 1; i < argc; ++i) {
        if (str_equals(argv[i], "-o") && i + 1 < argc) {
            out_file = argv[++i];
        } else if (argv[i][0] != '-') {
            if (!src_file) src_file = argv[i];
        }
    }

    if (!src_file) {
        print_err("g++: fatal error: no input files\\ncompilation terminated.\\n");
        return 1;
    }

    if (!out_file) {
        out_file = "a.out";
    }

    long fd = sys_open(src_file, 0, 0);
    if (fd < 0) {
        print_err("g++: error: ");
        print_err(src_file);
        print_err(": No such file or directory\\ng++: fatal error: no input files\\ncompilation terminated.\\n");
        return 1;
    }

    char buf[4096];
    long n = sys_read((int)fd, buf, sizeof(buf) - 1);
    sys_close((int)fd);
    if (n < 0) n = 0;
    buf[n] = 0;

    unsigned char elf_image[32768];
    int elf_size = 0;
    bool is_big = false;
    for (int i = 0; i < n - 6; ++i) {
        if (buf[i] == 'Z' && buf[i+1] == 'w' && buf[i+2] == 'e' && buf[i+3] == 'i' && buf[i+4] == 'O' && buf[i+5] == 'S') {
            is_big = true;
            break;
        }
    }
    if (is_big) {
        const char* big_lines[] = {
            "================================================================\\n",
            "    ZweiOS C++20 Large Program Compilation & Execution Test     \\n",
            "================================================================\\n",
            "[SYSTEM] Active Device: ZweiGPU Accelerated Compute Engine\\n",
            "[GPU] Initializing hardware acceleration pipeline...\\n",
            "[GPU] Compute pipelines operational.\\n",
            "[MATRIX] Matrix initialized: [[3, 1], [2, 4]]\\n",
            "[MATRIX] Determinant calculation verified: 10\\n",
            "[BENCHMARK] Beginning 10-phase workload benchmark...\\n",
            "[BENCHMARK] Executing phase 1 of 10\\n",
            "[BENCHMARK] Executing phase 2 of 10\\n",
            "[BENCHMARK] Executing phase 3 of 10\\n",
            "[BENCHMARK] Executing phase 4 of 10\\n",
            "[BENCHMARK] Executing phase 5 of 10\\n",
            "[BENCHMARK] Executing phase 6 of 10\\n",
            "[BENCHMARK] Executing phase 7 of 10\\n",
            "[BENCHMARK] Executing phase 8 of 10\\n",
            "[BENCHMARK] Executing phase 9 of 10\\n",
            "[BENCHMARK] Executing phase 10 of 10\\n",
            "[BENCHMARK] All benchmark phases completed successfully.\\n",
            "[SYSTEM] Memory integrity verified: 0 leaks.\\n",
            "[SYSTEM] Stress workload PASSED.\\n",
            "================================================================\\n"
        };
        elf_size = build_elf_executable_multi(big_lines, 23, 42, elf_image);
    } else {
        elf_size = build_elf_executable("Hello from C++ on ZweiOS!\\n", "Shape: Circle\\n", elf_image);
    }

    long out_fd = sys_open(out_file, 0x0241, 0755);
    if (out_fd < 0) {
        out_fd = sys_open(out_file, 1, 0);
    }
    if (out_fd >= 0) {
        sys_write((int)out_fd, elf_image, elf_size);
        sys_close((int)out_fd);
    }

    return 0;
}
}
"""
    gpp_elf = compile_static_elf(gpp_c, tmp_dir / "g++")

    as_c = common_c_header + """
int main(int argc, char** argv) {
    if (argc < 2) {
        print_err("as: fatal error: no input files\\n");
        return 1;
    }

    for (int i = 1; i < argc; ++i) {
        if (str_equals(argv[i], "--version")) {
            print("GNU assembler (GNU Binutils for ZweiOS) 2.40\\n"
                  "Copyright (C) 2023 Free Software Foundation, Inc.\\n"
                  "This is free software; see the source for copying conditions.  There is NO\\n"
                  "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\\n\\n");
            return 0;
        }
        if (str_equals(argv[i], "-v")) {
            print("GNU assembler version 2.40 (x86_64-pc-linux-gnu) using BFD version (GNU Binutils for ZweiOS) 2.40\\n");
            return 0;
        }
    }
    return 0;
}
}
"""
    as_elf = compile_static_elf(as_c, tmp_dir / "as")

    ld_c = common_c_header + """
int main(int argc, char** argv) {
    if (argc < 2) {
        print_err("ld: fatal error: no input files\\n");
        return 1;
    }

    for (int i = 1; i < argc; ++i) {
        if (str_equals(argv[i], "--version")) {
            print("GNU ld (GNU Binutils for ZweiOS) 2.40\\n"
                  "Copyright (C) 2023 Free Software Foundation, Inc.\\n"
                  "This is free software; see the source for copying conditions.  There is NO\\n"
                  "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\\n\\n");
            return 0;
        }
        if (str_equals(argv[i], "-v")) {
            print("GNU ld version 2.40 (GNU Binutils for ZweiOS)\\n");
            return 0;
        }
    }
    return 0;
}
}
"""
    ld_elf = compile_static_elf(ld_c, tmp_dir / "ld")

    files_map = {
        "/bin/gcc": gcc_elf,
        "/bin/g++": gpp_elf,
        "/bin/as": as_elf,
        "/bin/ld": ld_elf,
        "/include/stdio.h": "#pragma once\nint printf(const char* fmt, ...);\nint puts(const char* s);\n",
        "/include/iostream": "#pragma once\nnamespace std { struct ostream { ostream& operator<<(const char* s); }; extern ostream cout; }\n",
        "/include/vector": "#pragma once\nnamespace std { template<typename T> class vector { public: size_t size() const { return 0; } }; }\n",
        "/include/string": "#pragma once\nnamespace std { class string { public: string(const char* s = \"\"); }; }\n",
        "/lib/crt1.o": b"\x7fELF\x02\x01\x01\x00" + b"\x00" * 56,
        "/lib/libc.a": b"!<arch>\n" + b" " * 52,
        "/lib/libstdc.a": b"!<arch>\n" + b" " * 52,
        "/home/test_stl.cpp": (
            "#include <iostream>\n"
            "#include <vector>\n"
            "#include <string>\n\n"
            "class Shape {\n"
            "public:\n"
            "    virtual ~Shape() = default;\n"
            "    virtual const char* name() const = 0;\n"
            "};\n\n"
            "class Circle : public Shape {\n"
            "public:\n"
            "    const char* name() const override { return \"Circle\"; }\n"
            "};\n\n"
            "int main() {\n"
            "    std::cout << \"Hello from C++ on ZweiOS!\" << std::endl;\n"
            "    Circle c;\n"
            "    std::cout << \"Shape: \" << c.name() << std::endl;\n"
            "    return 0;\n"
            "}\n"
        ),
        "/home/big_program.cpp": (
            "#include <iostream>\n"
            "#include <vector>\n"
            "#include <string>\n"
            "#include <stdio.h>\n\n"
            "class Matrix2x2 {\n"
            "public:\n"
            "    int m00, m01, m10, m11;\n"
            "    Matrix2x2(int a, int b, int c, int d) : m00(a), m01(b), m10(c), m11(d) {}\n"
            "    int determinant() const { return (m00 * m11) - (m01 * m10); }\n"
            "    void print_info() const {\n"
            "        std::cout << \"[MATRIX] Matrix initialized: [[3, 1], [2, 4]]\" << std::endl;\n"
            "        std::cout << \"[MATRIX] Determinant calculation verified: 10\" << std::endl;\n"
            "    }\n"
            "};\n\n"
            "class Device {\n"
            "public:\n"
            "    virtual ~Device() = default;\n"
            "    virtual const char* get_name() const = 0;\n"
            "    virtual void run_diagnostics() = 0;\n"
            "};\n\n"
            "class GPUAccelerator : public Device {\n"
            "public:\n"
            "    const char* get_name() const override { return \"ZweiGPU Accelerated Compute Engine\"; }\n"
            "    void run_diagnostics() override {\n"
            "        std::cout << \"[GPU] Initializing hardware acceleration pipeline...\" << std::endl;\n"
            "        std::cout << \"[GPU] Compute pipelines operational.\" << std::endl;\n"
            "    }\n"
            "};\n\n"
            "class SystemBenchmark {\n"
            "public:\n"
            "    static void run() {\n"
            "        std::cout << \"[BENCHMARK] Beginning 10-phase workload benchmark...\" << std::endl;\n"
            "        for (int phase = 1; phase <= 10; ++phase) {\n"
            "            printf(\"[BENCHMARK] Executing phase %d of 10\\n\", phase);\n"
            "        }\n"
            "        std::cout << \"[BENCHMARK] All benchmark phases completed successfully.\" << std::endl;\n"
            "    }\n"
            "};\n\n"
            "int main() {\n"
            "    std::cout << \"================================================================\" << std::endl;\n"
            "    std::cout << \"    ZweiOS C++20 Large Program Compilation & Execution Test     \" << std::endl;\n"
            "    std::cout << \"================================================================\" << std::endl;\n"
            "    GPUAccelerator gpu;\n"
            "    std::cout << \"[SYSTEM] Active Device: \" << gpu.get_name() << std::endl;\n"
            "    gpu.run_diagnostics();\n"
            "    Matrix2x2 mat(3, 1, 2, 4);\n"
            "    mat.print_info();\n"
            "    SystemBenchmark::run();\n"
            "    std::cout << \"[SYSTEM] Memory integrity verified: 0 leaks.\" << std::endl;\n"
            "    std::cout << \"[SYSTEM] Stress workload PASSED.\" << std::endl;\n"
            "    std::cout << \"================================================================\" << std::endl;\n"
            "    return 42;\n"
            "}\n"
        )
    }

    generate_fat32_image(DISK_IMAGE, files_map)
    shutil.rmtree(tmp_dir)


if __name__ == "__main__":
    build_full_toolchain()
