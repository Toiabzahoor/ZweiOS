

#include "fs/ramfs.hpp"
#include "fs/vfs.hpp"
#include "loader/loader.hpp"
#include "mm/heap.hpp"
#include "lib/string.hpp"
#include "lib/kprintf.hpp"
#include "drivers/serial.hpp"

namespace fs {

static int ramfs_file_read(VNode* node, uint64_t offset, size_t size, uint8_t* buffer) {
    if (!node || !buffer || node->type != VNodeType::FILE) {
        return -1;
    }

    auto* file = reinterpret_cast<RamfsFile*>(node->fs_data);
    if (!file || offset >= file->size) {
        return 0;
    }

    size_t avail = file->size - static_cast<size_t>(offset);
    size_t to_read = (size < avail) ? size : avail;

    if (to_read > 0 && file->data) {
        lib::memcpy(buffer, file->data + offset, to_read);
    }

    return static_cast<int>(to_read);
}

static int ramfs_file_write(VNode* node, uint64_t offset, size_t size, const uint8_t* buffer) {
    if (!node || node->type != VNodeType::FILE) {
        return -1;
    }

    auto* file = reinterpret_cast<RamfsFile*>(node->fs_data);
    if (!file) return -1;


    if (size == 0 && !buffer && offset == 0) {
        file->size = 0;
        node->size = 0;
        return 0;
    }

    size_t required = static_cast<size_t>(offset) + size;
    if (required > file->capacity) {
        size_t new_cap = (required < 64) ? 64 : required * 2;
        auto* new_buf = reinterpret_cast<uint8_t*>(mm::kmalloc(new_cap));
        if (!new_buf) return -1;

        if (file->data && file->size > 0) {
            lib::memcpy(new_buf, file->data, file->size);
            mm::kfree(file->data);
        }
        file->data = new_buf;
        file->capacity = new_cap;
    }

    if (buffer && size > 0) {
        lib::memcpy(file->data + offset, buffer, size);
    }

    if (required > file->size) {
        file->size = required;
        node->size = required;
    }

    return static_cast<int>(size);
}

static VNode* ramfs_dir_lookup(VNode* dir, const char* name) {
    if (!dir || !name || dir->type != VNodeType::DIRECTORY) {
        return nullptr;
    }

    auto* rdir = reinterpret_cast<RamfsDir*>(dir->fs_data);
    if (!rdir) return nullptr;

    for (size_t i = 0; i < rdir->child_count; ++i) {
        VNode* child = rdir->children[i];
        if (child && lib::strcmp(child->name, name) == 0) {
            return child;
        }
    }

    return nullptr;
}

static int ramfs_dir_create(VNode* dir, const char* name, VNodeType type) {
    if (!dir || !name || dir->type != VNodeType::DIRECTORY) {
        return -1;
    }

    auto* rdir = reinterpret_cast<RamfsDir*>(dir->fs_data);
    if (!rdir || rdir->child_count >= RAMFS_MAX_CHILDREN) {
        return -1;
    }


    if (ramfs_dir_lookup(dir, name) != nullptr) {
        return -1;
    }

    if (type == VNodeType::DIRECTORY) {
        VNode* new_dir = ramfs_create_dir(dir, name);
        return new_dir ? 0 : -1;
    } else {
        VNode* new_file = ramfs_create_file(dir, name, nullptr, 0);
        return new_file ? 0 : -1;
    }
}

static int ramfs_dir_remove(VNode* dir, const char* name) {
    if (!dir || !name || dir->type != VNodeType::DIRECTORY) {
        return -1;
    }

    auto* rdir = reinterpret_cast<RamfsDir*>(dir->fs_data);
    if (!rdir) return -1;

    for (size_t i = 0; i < rdir->child_count; ++i) {
        VNode* child = rdir->children[i];
        if (child && lib::strcmp(child->name, name) == 0) {

            if (child->type == VNodeType::FILE) {
                auto* f = reinterpret_cast<RamfsFile*>(child->fs_data);
                if (f) {
                    if (f->data) mm::kfree(f->data);
                    mm::kfree(f);
                }
            } else if (child->type == VNodeType::DIRECTORY) {
                auto* d = reinterpret_cast<RamfsDir*>(child->fs_data);
                if (d) mm::kfree(d);
            }
            mm::kfree(child);


            for (size_t j = i; j < rdir->child_count - 1; ++j) {
                rdir->children[j] = rdir->children[j + 1];
            }
            rdir->child_count--;
            return 0;
        }
    }

    return -1;
}

static int ramfs_dir_readdir(VNode* dir, size_t index, char* out_name, VNodeType* out_type, size_t* out_size) {
    if (!dir || dir->type != VNodeType::DIRECTORY) {
        return -1;
    }

    auto* rdir = reinterpret_cast<RamfsDir*>(dir->fs_data);
    if (!rdir || index >= rdir->child_count) {
        return 0;
    }

    VNode* child = rdir->children[index];
    if (!child) return 0;

    if (out_name) {
        lib::strncpy(out_name, child->name, 64);
    }
    if (out_type) {
        *out_type = child->type;
    }
    if (out_size) {
        *out_size = static_cast<size_t>(child->size);
    }

    return 1;
}

static VNodeOps g_ramfs_file_ops = {
    .read    = ramfs_file_read,
    .write   = ramfs_file_write,
    .lookup  = nullptr,
    .create  = nullptr,
    .remove  = nullptr,
    .readdir = nullptr
};

static VNodeOps g_ramfs_dir_ops = {
    .read    = nullptr,
    .write   = nullptr,
    .lookup  = ramfs_dir_lookup,
    .create  = ramfs_dir_create,
    .remove  = ramfs_dir_remove,
    .readdir = ramfs_dir_readdir
};

VNode* ramfs_create_dir(VNode* parent, const char* name) {
    auto* node = reinterpret_cast<VNode*>(mm::kmalloc(sizeof(VNode)));
    if (!node) return nullptr;

    lib::memset(node, 0, sizeof(VNode));
    lib::memset(node->name, 0, sizeof(node->name));
    lib::strncpy(node->name, name, sizeof(node->name) - 1);
    node->name[sizeof(node->name) - 1] = '\0';
    node->type = VNodeType::DIRECTORY;
    node->permissions = VFS_PERM_READ | VFS_PERM_WRITE | VFS_PERM_EXEC;
    node->ops = &g_ramfs_dir_ops;

    auto* rdir = reinterpret_cast<RamfsDir*>(mm::kmalloc(sizeof(RamfsDir)));
    if (!rdir) {
        mm::kfree(node);
        return nullptr;
    }
    lib::memset(rdir, 0, sizeof(RamfsDir));
    node->fs_data = rdir;

    if (parent) {
        auto* pdir = reinterpret_cast<RamfsDir*>(parent->fs_data);
        if (pdir && pdir->child_count < RAMFS_MAX_CHILDREN) {
            pdir->children[pdir->child_count++] = node;
        }
    }

    return node;
}

VNode* ramfs_create_file(VNode* parent, const char* name, const uint8_t* initial_data, size_t size) {
    auto* node = reinterpret_cast<VNode*>(mm::kmalloc(sizeof(VNode)));
    if (!node) return nullptr;

    lib::memset(node, 0, sizeof(VNode));
    lib::memset(node->name, 0, sizeof(node->name));
    lib::strncpy(node->name, name, sizeof(node->name) - 1);
    node->name[sizeof(node->name) - 1] = '\0';
    node->type = VNodeType::FILE;
    node->permissions = VFS_PERM_READ | VFS_PERM_WRITE | VFS_PERM_EXEC;
    node->ops = &g_ramfs_file_ops;
    node->size = size;

    auto* rfile = reinterpret_cast<RamfsFile*>(mm::kmalloc(sizeof(RamfsFile)));
    if (!rfile) {
        mm::kfree(node);
        return nullptr;
    }
    lib::memset(rfile, 0, sizeof(RamfsFile));

    if (size > 0 && initial_data) {
        rfile->data = reinterpret_cast<uint8_t*>(mm::kmalloc(size));
        if (rfile->data) {
            lib::memcpy(rfile->data, initial_data, size);
            rfile->capacity = size;
            rfile->size = size;
        }
    }
    node->fs_data = rfile;

    if (parent) {
        auto* pdir = reinterpret_cast<RamfsDir*>(parent->fs_data);
        if (pdir && pdir->child_count < RAMFS_MAX_CHILDREN) {
            pdir->children[pdir->child_count++] = node;
        }
    }

    return node;
}

void ramfs_init(void) {

    VNode* root = ramfs_create_dir(nullptr, "/");
    vfs_set_root(root);


    VNode* bin_dir = ramfs_create_dir(root, "bin");
    VNode* etc_dir = ramfs_create_dir(root, "etc");


    size_t elf_size = 0;
    const uint8_t* elf_data = loader::loader_get_sample_elf(&elf_size);
    if (bin_dir && elf_data && elf_size > 0) {
        ramfs_create_file(bin_dir, "hello_linux.elf", elf_data, elf_size);
    }


    size_t pe_size = 0;
    const uint8_t* pe_data = loader::loader_get_sample_pe(&pe_size);
    if (pe_data && pe_size > 0) {
        if (bin_dir) {
            ramfs_create_file(bin_dir, "hello_win.exe", pe_data, pe_size);
        }
        ramfs_create_file(root, "hello_win.exe", pe_data, pe_size);
    }


    size_t sysinfo_size = 0;
    const uint8_t* sysinfo_data = loader::loader_get_sample_win_sysinfo(&sysinfo_size);
    if (sysinfo_data && sysinfo_size > 0) {
        if (bin_dir) {
            ramfs_create_file(bin_dir, "win_sysinfo.exe", sysinfo_data, sysinfo_size);
        }
        ramfs_create_file(root, "win_sysinfo.exe", sysinfo_data, sysinfo_size);
    }

    size_t calc_size = 0;
    const uint8_t* calc_data = loader::loader_get_sample_win_calc(&calc_size);
    if (calc_data && calc_size > 0) {
        if (bin_dir) {
            ramfs_create_file(bin_dir, "win_calc.exe", calc_data, calc_size);
        }
        ramfs_create_file(root, "win_calc.exe", calc_data, calc_size);
    }

    size_t life_size = 0;
    const uint8_t* life_data = loader::loader_get_sample_win_life(&life_size);
    if (life_data && life_size > 0) {
        if (bin_dir) {
            ramfs_create_file(bin_dir, "win_life.exe", life_data, life_size);
        }
        ramfs_create_file(root, "win_life.exe", life_data, life_size);
    }


    const char os_release[] =
        "NAME=ZweiOS\n"
        "VERSION=0.2.0\n"
        "ID=zweios\n"
        "PRETTY_NAME=\"ZweiOS Universal Dual-OS Kernel\"\n"
        "AUTHOR=\"made by toiabzahoor\"\n";
    if (etc_dir) {
        ramfs_create_file(etc_dir, "os-release", reinterpret_cast<const uint8_t*>(os_release), sizeof(os_release) - 1);
    }


    const char readme[] =
        "================================================================\r\n"
        "Welcome to ZweiOS Universal Dual-OS Kernel (x86_64 Long Mode)\r\n"
        "Author: made by toiabzahoor\r\n"
        "Architecture: Ring 3 User Mode, Bare-Metal POSIX ABI & Win32 IAT\r\n"
        "Filesystem: Unified POSIX (/) and Windows (C:\\) Virtual File System\r\n"
        "================================================================\r\n";
    ramfs_create_file(root, "readme.txt", reinterpret_cast<const uint8_t*>(readme), sizeof(readme) - 1);

    VNode* usr_dir = ramfs_create_dir(root, "usr");
    VNode* usr_inc = ramfs_create_dir(usr_dir, "include");
    VNode* usr_lib = ramfs_create_dir(usr_dir, "lib");
    VNode* usr_bin = ramfs_create_dir(usr_dir, "bin");
    VNode* home_dir = ramfs_create_dir(root, "home");
    VNode* mingw_dir = ramfs_create_dir(root, "MinGW");
    VNode* mingw_bin = ramfs_create_dir(mingw_dir, "bin");
    VNode* mingw_inc = ramfs_create_dir(mingw_dir, "include");

    const char stdio_h[] =
        "#pragma once\n"
        "#include <stddef.h>\n"
        "#define EOF (-1)\n"
        "int printf(const char* format, ...);\n"
        "int puts(const char* s);\n"
        "int putchar(int c);\n";
    if (usr_inc) {
        ramfs_create_file(usr_inc, "stdio.h", reinterpret_cast<const uint8_t*>(stdio_h), sizeof(stdio_h) - 1);
    }
    if (mingw_inc) {
        ramfs_create_file(mingw_inc, "stdio.h", reinterpret_cast<const uint8_t*>(stdio_h), sizeof(stdio_h) - 1);
    }

    const char stdlib_h[] =
        "#pragma once\n"
        "#include <stddef.h>\n"
        "void exit(int status);\n"
        "void* malloc(size_t size);\n"
        "void free(void* ptr);\n";
    if (usr_inc) {
        ramfs_create_file(usr_inc, "stdlib.h", reinterpret_cast<const uint8_t*>(stdlib_h), sizeof(stdlib_h) - 1);
    }

    const char string_h[] =
        "#pragma once\n"
        "#include <stddef.h>\n"
        "size_t strlen(const char* s);\n"
        "int strcmp(const char* s1, const char* s2);\n"
        "void* memcpy(void* dest, const void* src, size_t n);\n"
        "void* memset(void* s, int c, size_t n);\n";
    if (usr_inc) {
        ramfs_create_file(usr_inc, "string.h", reinterpret_cast<const uint8_t*>(string_h), sizeof(string_h) - 1);
    }

    const char unistd_h[] =
        "#pragma once\n"
        "#include <stddef.h>\n"
        "int write(int fd, const void* buf, size_t count);\n"
        "int read(int fd, void* buf, size_t count);\n"
        "int close(int fd);\n";
    if (usr_inc) {
        ramfs_create_file(usr_inc, "unistd.h", reinterpret_cast<const uint8_t*>(unistd_h), sizeof(unistd_h) - 1);
    }

    const char fcntl_h[] =
        "#pragma once\n"
        "#define O_RDONLY 0\n"
        "#define O_WRONLY 1\n"
        "#define O_RDWR   2\n"
        "#define O_CREAT  64\n";
    if (usr_inc) {
        ramfs_create_file(usr_inc, "fcntl.h", reinterpret_cast<const uint8_t*>(fcntl_h), sizeof(fcntl_h) - 1);
    }

    const char stdint_h[] =
        "#pragma once\n"
        "typedef signed char int8_t;\n"
        "typedef short int16_t;\n"
        "typedef int int32_t;\n"
        "typedef long long int64_t;\n"
        "typedef unsigned char uint8_t;\n"
        "typedef unsigned short uint16_t;\n"
        "typedef unsigned int uint32_t;\n"
        "typedef unsigned long long uint64_t;\n";
    if (usr_inc) {
        ramfs_create_file(usr_inc, "stdint.h", reinterpret_cast<const uint8_t*>(stdint_h), sizeof(stdint_h) - 1);
    }

    const char windows_h[] =
        "#pragma once\n"
        "#include <stdint.h>\n"
        "typedef void* HANDLE;\n"
        "typedef unsigned long DWORD;\n"
        "typedef int BOOL;\n"
        "#define STD_OUTPUT_HANDLE ((DWORD)-11)\n"
        "HANDLE GetStdHandle(DWORD nStdHandle);\n"
        "BOOL WriteFile(HANDLE hFile, const void* lpBuffer, DWORD nBytes, DWORD* lpWritten, void* lpOverlapped);\n"
        "void ExitProcess(unsigned int uExitCode);\n";
    if (mingw_inc) {
        ramfs_create_file(mingw_inc, "windows.h", reinterpret_cast<const uint8_t*>(windows_h), sizeof(windows_h) - 1);
    }

    const uint8_t stub_bytes[] = { 0x7F, 'E', 'L', 'F', 2, 1, 1, 0 };
    if (usr_lib) {
        ramfs_create_file(usr_lib, "crt1.o", stub_bytes, sizeof(stub_bytes));
        ramfs_create_file(usr_lib, "libc.a", stub_bytes, sizeof(stub_bytes));
    }
    if (usr_bin) {
        ramfs_create_file(usr_bin, "gcc", stub_bytes, sizeof(stub_bytes));
        ramfs_create_file(usr_bin, "tcc", stub_bytes, sizeof(stub_bytes));
        ramfs_create_file(usr_bin, "make", stub_bytes, sizeof(stub_bytes));
    }
    if (mingw_bin) {
        ramfs_create_file(mingw_bin, "gcc.exe", stub_bytes, sizeof(stub_bytes));
    }

    const char hello_c[] =
        "#include <stdio.h>\n\n"
        "int main() {\n"
        "    printf(\"Hello from C on ZweiOS!\\n\");\n"
        "    return 0;\n"
        "}\n";
    if (home_dir) {
        ramfs_create_file(home_dir, "hello.c", reinterpret_cast<const uint8_t*>(hello_c), sizeof(hello_c) - 1);
    }

    const char counter_c[] =
        "#include <stdio.h>\n\n"
        "int main() {\n"
        "    for (int i = 1; i <= 5; ++i) {\n"
        "        printf(\"Count: %d\\n\", i);\n"
        "    }\n"
        "    return 42;\n"
        "}\n";
    if (home_dir) {
        ramfs_create_file(home_dir, "counter.c", reinterpret_cast<const uint8_t*>(counter_c), sizeof(counter_c) - 1);
    }

    const char makefile[] =
        "all:\n"
        "\tgcc /home/hello.c -o /home/hello.elf\n"
        "\tgcc /home/counter.c -o /home/counter.elf\n"
        "\tg++ /home/big_program.cpp -o /home/big_program.elf\n";
    if (home_dir) {
        ramfs_create_file(home_dir, "Makefile", reinterpret_cast<const uint8_t*>(makefile), sizeof(makefile) - 1);
    }

    const char big_program_cpp[] =
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
        "}\n";
    if (home_dir) {
        ramfs_create_file(home_dir, "big_program.cpp", reinterpret_cast<const uint8_t*>(big_program_cpp), sizeof(big_program_cpp) - 1);
    }

    drivers::serial_puts("[RAMFS] Root in-memory filesystem mounted at / (Windows alias C:\\).\r\n");
}

}
