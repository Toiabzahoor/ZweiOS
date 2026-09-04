/* ==============================================================================
 * ZweiOS - Bare-Metal x86_64 Operating System
 * Component: In-Memory Root RAMFS Implementation
 * ============================================================================== */

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
        return 0; // EOF
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

    // Truncate case
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

    // Check if entry already exists
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
            // Free resources
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

            // Shift remaining children
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
        return 0; // End of directory
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
    lib::strncpy(node->name, name, sizeof(node->name) - 1);
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
    lib::strncpy(node->name, name, sizeof(node->name) - 1);
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
    // 1. Create root "/" directory
    VNode* root = ramfs_create_dir(nullptr, "/");
    vfs_set_root(root);

    // 2. Create standard system directories
    VNode* bin_dir = ramfs_create_dir(root, "bin");
    VNode* etc_dir = ramfs_create_dir(root, "etc");

    // 3. Pre-populate standalone Linux ELF64 binary
    size_t elf_size = 0;
    const uint8_t* elf_data = loader::loader_get_sample_elf(&elf_size);
    if (bin_dir && elf_data && elf_size > 0) {
        ramfs_create_file(bin_dir, "hello_linux.elf", elf_data, elf_size);
    }

    // 4. Pre-populate standalone Windows PE32+ binary (both in /bin and root alias)
    size_t pe_size = 0;
    const uint8_t* pe_data = loader::loader_get_sample_pe(&pe_size);
    if (pe_data && pe_size > 0) {
        if (bin_dir) {
            ramfs_create_file(bin_dir, "hello_win.exe", pe_data, pe_size);
        }
        ramfs_create_file(root, "hello_win.exe", pe_data, pe_size);
    }

    // 4b. Pre-populate complex Windows PE32+ forensic & benchmark applications
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

    // 5. System metadata file (/etc/os-release)
    const char os_release[] =
        "NAME=ZweiOS\n"
        "VERSION=0.2.0\n"
        "ID=zweios\n"
        "PRETTY_NAME=\"ZweiOS Universal Dual-OS Kernel\"\n"
        "AUTHOR=\"made by toiabzahoor\"\n";
    if (etc_dir) {
        ramfs_create_file(etc_dir, "os-release", reinterpret_cast<const uint8_t*>(os_release), sizeof(os_release) - 1);
    }

    // 6. Introductory documentation (/readme.txt)
    const char readme[] =
        "================================================================\r\n"
        "Welcome to ZweiOS Universal Dual-OS Kernel (x86_64 Long Mode)\r\n"
        "Author: made by toiabzahoor\r\n"
        "Architecture: Ring 3 User Mode, Bare-Metal POSIX ABI & Win32 IAT\r\n"
        "Filesystem: Unified POSIX (/) and Windows (C:\\) Virtual File System\r\n"
        "================================================================\r\n";
    ramfs_create_file(root, "readme.txt", reinterpret_cast<const uint8_t*>(readme), sizeof(readme) - 1);

    drivers::serial_puts("[RAMFS] Root in-memory filesystem mounted at / (Windows alias C:\\).\r\n");
}

} // namespace fs
