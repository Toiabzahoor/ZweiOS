#include "fs/fat32.hpp"
#include "drivers/ata.hpp"
#include "mm/heap.hpp"
#include "lib/string.hpp"
#include "lib/kprintf.hpp"

namespace fs {

static Fat32Instance g_fat32_instances[4];
static size_t        g_fat32_instance_count = 0;

static int    fat32_vnode_read(VNode* node, uint64_t offset, size_t size, uint8_t* buffer);
static int    fat32_vnode_write(VNode* node, uint64_t offset, size_t size, const uint8_t* buffer);
static VNode* fat32_vnode_lookup(VNode* dir, const char* name);
static int    fat32_vnode_create(VNode* dir, const char* name, VNodeType type);
static int    fat32_vnode_remove(VNode* dir, const char* name);
static int    fat32_vnode_readdir(VNode* dir, size_t index, char* out_name, VNodeType* out_type, size_t* out_size);

static VNodeOps g_fat32_ops = {
    .read    = fat32_vnode_read,
    .write   = fat32_vnode_write,
    .lookup  = fat32_vnode_lookup,
    .create  = fat32_vnode_create,
    .remove  = fat32_vnode_remove,
    .readdir = fat32_vnode_readdir,
};

void fat32_init(void) {
    g_fat32_instance_count = 0;
    for (size_t i = 0; i < 4; ++i) {
        g_fat32_instances[i].mounted = false;
    }
}

static uint32_t fat32_cluster_to_lba(Fat32Instance* fs, uint32_t cluster) {
    return static_cast<uint32_t>(fs->data_start_lba + static_cast<uint64_t>(cluster - 2) * fs->sectors_per_cluster);
}

static uint32_t fat32_get_next_cluster(Fat32Instance* fs, uint32_t cluster) {
    if (cluster < 2 || cluster >= FAT32_CLUSTER_BAD) {
        return FAT32_CLUSTER_EOC;
    }

    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = static_cast<uint32_t>(fs->fat_start_lba + (fat_offset / 512));
    uint32_t ent_offset = fat_offset % 512;

    uint8_t sector_buf[512];
    if (!drivers::ata_read_sectors(fat_sector, 1, sector_buf)) {
        return FAT32_CLUSTER_BAD;
    }

    uint32_t next = *reinterpret_cast<uint32_t*>(&sector_buf[ent_offset]) & 0x0FFFFFFF;
    return next;
}

static bool fat32_set_next_cluster(Fat32Instance* fs, uint32_t cluster, uint32_t next) {
    if (cluster < 2) return false;

    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector_rel = fat_offset / 512;
    uint32_t ent_offset = fat_offset % 512;

    uint8_t sector_buf[512];
    for (uint32_t f = 0; f < fs->fat_count; ++f) {
        uint32_t fat_sector = static_cast<uint32_t>(fs->fat_start_lba + (f * fs->fat_size_sectors) + fat_sector_rel);
        if (!drivers::ata_read_sectors(fat_sector, 1, sector_buf)) {
            return false;
        }

        uint32_t val = *reinterpret_cast<uint32_t*>(&sector_buf[ent_offset]);
        val = (val & 0xF0000000) | (next & 0x0FFFFFFF);
        *reinterpret_cast<uint32_t*>(&sector_buf[ent_offset]) = val;

        if (!drivers::ata_write_sectors(fat_sector, 1, sector_buf)) {
            return false;
        }
    }

    return true;
}

static uint32_t fat32_allocate_cluster(Fat32Instance* fs, uint32_t prev_cluster) {
    uint8_t sector_buf[512];
    uint32_t total_fat_sectors = fs->fat_size_sectors;

    for (uint32_t sec = 0; sec < total_fat_sectors; ++sec) {
        uint32_t fat_sector = static_cast<uint32_t>(fs->fat_start_lba + sec);
        if (!drivers::ata_read_sectors(fat_sector, 1, sector_buf)) {
            return 0;
        }

        for (uint32_t i = 0; i < 512; i += 4) {
            uint32_t cluster_idx = (sec * (512 / 4)) + (i / 4);
            if (cluster_idx < 2) continue;

            uint32_t val = *reinterpret_cast<uint32_t*>(&sector_buf[i]) & 0x0FFFFFFF;
            if (val == FAT32_CLUSTER_FREE) {
                fat32_set_next_cluster(fs, cluster_idx, FAT32_CLUSTER_EOC);

                if (prev_cluster >= 2) {
                    fat32_set_next_cluster(fs, prev_cluster, cluster_idx);
                }

                uint8_t zero_buf[512];
                lib::memset(zero_buf, 0, 512);
                uint32_t clus_lba = fat32_cluster_to_lba(fs, cluster_idx);
                for (uint32_t s = 0; s < fs->sectors_per_cluster; ++s) {
                    drivers::ata_write_sectors(clus_lba + s, 1, zero_buf);
                }

                return cluster_idx;
            }
        }
    }

    return 0;
}

static void fat32_free_cluster_chain(Fat32Instance* fs, uint32_t start_cluster) {
    uint32_t curr = start_cluster;
    while (curr >= 2 && curr < FAT32_CLUSTER_EOC) {
        uint32_t next = fat32_get_next_cluster(fs, curr);
        fat32_set_next_cluster(fs, curr, FAT32_CLUSTER_FREE);
        curr = next;
    }
}

static void format_8_3_name(const char* fat_name, char* out_name, size_t max_len) {
    if (!fat_name || !out_name || max_len == 0) return;

    char base[9];
    char ext[4];
    size_t b_idx = 0;
    size_t e_idx = 0;

    for (size_t i = 0; i < 8; ++i) {
        if (fat_name[i] != ' ') {
            base[b_idx++] = fat_name[i];
        }
    }
    base[b_idx] = '\0';

    for (size_t i = 8; i < 11; ++i) {
        if (fat_name[i] != ' ') {
            ext[e_idx++] = fat_name[i];
        }
    }
    ext[e_idx] = '\0';

    size_t out_idx = 0;
    for (size_t i = 0; i < b_idx && out_idx < max_len - 1; ++i) {
        char c = base[i];
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
        out_name[out_idx++] = c;
    }

    if (e_idx > 0 && out_idx < max_len - 1) {
        out_name[out_idx++] = '.';
        for (size_t i = 0; i < e_idx && out_idx < max_len - 1; ++i) {
            char c = ext[i];
            if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
            out_name[out_idx++] = c;
        }
    }

    out_name[out_idx] = '\0';
}

static void create_8_3_name(const char* in_name, char* out_fat_name) {
    for (size_t i = 0; i < 11; ++i) {
        out_fat_name[i] = ' ';
    }

    const char* dot = nullptr;
    size_t len = lib::strlen(in_name);
    for (size_t i = 0; i < len; ++i) {
        if (in_name[i] == '.') {
            dot = &in_name[i];
        }
    }

    size_t base_len = dot ? static_cast<size_t>(dot - in_name) : len;
    if (base_len > 8) base_len = 8;

    for (size_t i = 0; i < base_len; ++i) {
        char c = in_name[i];
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - ('a' - 'A'));
        out_fat_name[i] = c;
    }

    if (dot) {
        const char* ext = dot + 1;
        size_t ext_len = lib::strlen(ext);
        if (ext_len > 3) ext_len = 3;
        for (size_t i = 0; i < ext_len; ++i) {
            char c = ext[i];
            if (c >= 'a' && c <= 'z') c = static_cast<char>(c - ('a' - 'A'));
            out_fat_name[8 + i] = c;
        }
    }
}

static bool str_equals_ignore_case(const char* s1, const char* s2) {
    while (*s1 && *s2) {
        char c1 = *s1;
        char c2 = *s2;
        if (c1 >= 'A' && c1 <= 'Z') c1 = static_cast<char>(c1 + ('a' - 'A'));
        if (c2 >= 'A' && c2 <= 'Z') c2 = static_cast<char>(c2 + ('a' - 'A'));
        if (c1 != c2) return false;
        s1++;
        s2++;
    }
    return (*s1 == '\0' && *s2 == '\0');
}

bool fat32_probe(uint32_t drive_id, uint64_t start_lba, Fat32Instance* out_fs) {
    if (!out_fs) return false;

    uint8_t sector[512];
    if (!drivers::ata_read_sectors(static_cast<uint32_t>(start_lba), 1, sector)) {
        return false;
    }

    auto* bpb = reinterpret_cast<const Fat32BootSector*>(sector);
    if (bpb->signature != 0xAA55) {
        return false;
    }

    if (bpb->bytes_per_sector != 512 || bpb->sectors_per_cluster == 0 || bpb->reserved_sectors == 0 || bpb->fat_count == 0) {
        return false;
    }

    uint32_t fat_sz = (bpb->fat_size_16 != 0) ? bpb->fat_size_16 : bpb->fat_size_32;
    if (fat_sz == 0 || bpb->root_cluster < 2) {
        return false;
    }

    out_fs->drive_id = drive_id;
    out_fs->partition_start_lba = start_lba;
    out_fs->bytes_per_sector = bpb->bytes_per_sector;
    out_fs->sectors_per_cluster = bpb->sectors_per_cluster;
    out_fs->bytes_per_cluster = bpb->bytes_per_sector * bpb->sectors_per_cluster;
    out_fs->reserved_sectors = bpb->reserved_sectors;
    out_fs->fat_count = bpb->fat_count;
    out_fs->fat_size_sectors = fat_sz;
    out_fs->fat_start_lba = static_cast<uint32_t>(start_lba + bpb->reserved_sectors);
    out_fs->data_start_lba = static_cast<uint32_t>(out_fs->fat_start_lba + (bpb->fat_count * fat_sz));
    out_fs->root_cluster = bpb->root_cluster;

    uint32_t tot_sec = (bpb->total_sectors_16 != 0) ? bpb->total_sectors_16 : bpb->total_sectors_32;
    uint32_t data_sec = tot_sec - (out_fs->data_start_lba - static_cast<uint32_t>(start_lba));
    out_fs->total_clusters = data_sec / bpb->sectors_per_cluster;

    lib::memcpy(out_fs->volume_label, bpb->volume_label, 11);
    out_fs->volume_label[11] = '\0';
    for (int i = 10; i >= 0; --i) {
        if (out_fs->volume_label[i] == ' ') {
            out_fs->volume_label[i] = '\0';
        } else {
            break;
        }
    }

    out_fs->mounted = true;
    return true;
}

static int fat32_vnode_read(VNode* node, uint64_t offset, size_t size, uint8_t* buffer) {
    if (!node || !node->fs_data || !buffer || size == 0) {
        return 0;
    }

    auto* data = reinterpret_cast<FatNodeData*>(node->fs_data);
    Fat32Instance* fs = data->fs;

    if (!data->is_directory && offset >= node->size) {
        return 0;
    }

    if (!data->is_directory && (offset + size > node->size)) {
        size = static_cast<size_t>(node->size - offset);
    }

    uint32_t bpc = fs->bytes_per_cluster;
    uint32_t cluster_idx = static_cast<uint32_t>(offset / bpc);
    uint32_t cluster_offset = static_cast<uint32_t>(offset % bpc);

    uint32_t curr_cluster = data->first_cluster;
    for (uint32_t c = 0; c < cluster_idx; ++c) {
        if (curr_cluster < 2 || curr_cluster >= FAT32_CLUSTER_EOC) {
            return 0;
        }
        curr_cluster = fat32_get_next_cluster(fs, curr_cluster);
    }

    size_t bytes_read = 0;
    uint8_t cluster_buf[4096];

    while (bytes_read < size && curr_cluster >= 2 && curr_cluster < FAT32_CLUSTER_EOC) {
        uint32_t clus_lba = fat32_cluster_to_lba(fs, curr_cluster);
        for (uint32_t s = 0; s < fs->sectors_per_cluster; ++s) {
            if (!drivers::ata_read_sectors(clus_lba + s, 1, cluster_buf + (s * 512))) {
                return static_cast<int>(bytes_read);
            }
        }

        size_t available_in_cluster = bpc - cluster_offset;
        size_t to_copy = size - bytes_read;
        if (to_copy > available_in_cluster) {
            to_copy = available_in_cluster;
        }

        lib::memcpy(buffer + bytes_read, cluster_buf + cluster_offset, to_copy);
        bytes_read += to_copy;
        cluster_offset = 0;

        curr_cluster = fat32_get_next_cluster(fs, curr_cluster);
    }

    return static_cast<int>(bytes_read);
}

static int fat32_vnode_write(VNode* node, uint64_t offset, size_t size, const uint8_t* buffer) {
    if (!node || !node->fs_data) {
        return -1;
    }

    auto* data = reinterpret_cast<FatNodeData*>(node->fs_data);
    Fat32Instance* fs = data->fs;

    if (data->first_cluster == 0) {
        data->first_cluster = fat32_allocate_cluster(fs, 0);
        if (data->first_cluster == 0) {
            return -1;
        }

        if (data->dir_cluster != 0) {
            uint32_t dir_lba = fat32_cluster_to_lba(fs, data->dir_cluster);
            uint32_t sec_offset = (data->dir_entry_offset / 512);
            uint32_t ent_in_sec = data->dir_entry_offset % 512;

            uint8_t sec_buf[512];
            if (drivers::ata_read_sectors(dir_lba + sec_offset, 1, sec_buf)) {
                auto* entry = reinterpret_cast<FatDirectoryEntry*>(sec_buf + ent_in_sec);
                entry->cluster_low = static_cast<uint16_t>(data->first_cluster & 0xFFFF);
                entry->cluster_high = static_cast<uint16_t>((data->first_cluster >> 16) & 0xFFFF);
                drivers::ata_write_sectors(dir_lba + sec_offset, 1, sec_buf);
            }
        }
    }

    if (size == 0 || !buffer) {
        return 0;
    }

    uint32_t bpc = fs->bytes_per_cluster;
    uint32_t cluster_idx = static_cast<uint32_t>(offset / bpc);
    uint32_t cluster_offset = static_cast<uint32_t>(offset % bpc);

    uint32_t curr_cluster = data->first_cluster;
    for (uint32_t c = 0; c < cluster_idx; ++c) {
        uint32_t next = fat32_get_next_cluster(fs, curr_cluster);
        if (next < 2 || next >= FAT32_CLUSTER_EOC) {
            next = fat32_allocate_cluster(fs, curr_cluster);
            if (next == 0) {
                return -1;
            }
        }
        curr_cluster = next;
    }

    size_t bytes_written = 0;
    uint8_t cluster_buf[4096];

    while (bytes_written < size) {
        uint32_t clus_lba = fat32_cluster_to_lba(fs, curr_cluster);
        for (uint32_t s = 0; s < fs->sectors_per_cluster; ++s) {
            drivers::ata_read_sectors(clus_lba + s, 1, cluster_buf + (s * 512));
        }

        size_t available_in_cluster = bpc - cluster_offset;
        size_t to_copy = size - bytes_written;
        if (to_copy > available_in_cluster) {
            to_copy = available_in_cluster;
        }

        lib::memcpy(cluster_buf + cluster_offset, buffer + bytes_written, to_copy);

        for (uint32_t s = 0; s < fs->sectors_per_cluster; ++s) {
            drivers::ata_write_sectors(clus_lba + s, 1, cluster_buf + (s * 512));
        }

        bytes_written += to_copy;
        cluster_offset = 0;

        if (bytes_written < size) {
            uint32_t next = fat32_get_next_cluster(fs, curr_cluster);
            if (next < 2 || next >= FAT32_CLUSTER_EOC) {
                next = fat32_allocate_cluster(fs, curr_cluster);
                if (next == 0) {
                    break;
                }
            }
            curr_cluster = next;
        }
    }

    if (offset + bytes_written > node->size) {
        node->size = offset + bytes_written;

        if (data->dir_cluster != 0) {
            uint32_t dir_lba = fat32_cluster_to_lba(fs, data->dir_cluster);
            uint32_t sec_offset = (data->dir_entry_offset / 512);
            uint32_t ent_in_sec = data->dir_entry_offset % 512;

            uint8_t sec_buf[512];
            if (drivers::ata_read_sectors(dir_lba + sec_offset, 1, sec_buf)) {
                auto* entry = reinterpret_cast<FatDirectoryEntry*>(sec_buf + ent_in_sec);
                entry->file_size = static_cast<uint32_t>(node->size);
                drivers::ata_write_sectors(dir_lba + sec_offset, 1, sec_buf);
            }
        }
    }

    return static_cast<int>(bytes_written);
}

static int fat32_vnode_readdir(VNode* dir, size_t index, char* out_name, VNodeType* out_type, size_t* out_size) {
    if (!dir || !dir->fs_data || !out_name || !out_type || !out_size) {
        return 0;
    }

    auto* data = reinterpret_cast<FatNodeData*>(dir->fs_data);
    Fat32Instance* fs = data->fs;

    uint32_t curr_cluster = data->first_cluster;
    uint8_t cluster_buf[4096];
    char lfn_buf[256];
    lfn_buf[0] = '\0';
    bool has_lfn = false;

    size_t valid_entry_count = 0;

    while (curr_cluster >= 2 && curr_cluster < FAT32_CLUSTER_EOC) {
        uint32_t clus_lba = fat32_cluster_to_lba(fs, curr_cluster);
        for (uint32_t s = 0; s < fs->sectors_per_cluster; ++s) {
            if (!drivers::ata_read_sectors(clus_lba + s, 1, cluster_buf + (s * 512))) {
                return 0;
            }
        }

        size_t entries_per_cluster = fs->bytes_per_cluster / sizeof(FatDirectoryEntry);
        auto* entries = reinterpret_cast<FatDirectoryEntry*>(cluster_buf);

        for (size_t i = 0; i < entries_per_cluster; ++i) {
            const auto& ent = entries[i];
            uint8_t first_byte = static_cast<uint8_t>(ent.name[0]);

            if (first_byte == 0x00) {
                return 0;
            }

            if (first_byte == 0xE5) {
                has_lfn = false;
                lfn_buf[0] = '\0';
                continue;
            }

            if (ent.attributes == FAT_ATTR_LFN) {
                auto* lfn = reinterpret_cast<const FatLfnEntry*>(&ent);
                uint8_t seq = lfn->sequence_number & 0x1F;
                if (seq > 0 && seq <= 20) {
                    size_t char_pos = (seq - 1) * 13;
                    for (int c = 0; c < 5; ++c) {
                        uint16_t ch = lfn->name1[c];
                        if (ch == 0 || ch == 0xFFFF) break;
                        if (char_pos + c < sizeof(lfn_buf) - 1) lfn_buf[char_pos + c] = static_cast<char>(ch);
                    }
                    for (int c = 0; c < 6; ++c) {
                        uint16_t ch = lfn->name2[c];
                        if (ch == 0 || ch == 0xFFFF) break;
                        if (char_pos + 5 + c < sizeof(lfn_buf) - 1) lfn_buf[char_pos + 5 + c] = static_cast<char>(ch);
                    }
                    for (int c = 0; c < 2; ++c) {
                        uint16_t ch = lfn->name3[c];
                        if (ch == 0 || ch == 0xFFFF) break;
                        if (char_pos + 11 + c < sizeof(lfn_buf) - 1) lfn_buf[char_pos + 11 + c] = static_cast<char>(ch);
                    }
                    if (lfn->sequence_number & 0x40) {
                        lfn_buf[char_pos + 13] = '\0';
                    }
                    has_lfn = true;
                }
                continue;
            }

            if (ent.attributes & FAT_ATTR_VOLUME_ID) {
                has_lfn = false;
                lfn_buf[0] = '\0';
                continue;
            }

            if (ent.name[0] == '.' && (ent.name[1] == ' ' || (ent.name[1] == '.' && ent.name[2] == ' '))) {
                has_lfn = false;
                lfn_buf[0] = '\0';
                continue;
            }

            if (valid_entry_count == index) {
                if (has_lfn && lfn_buf[0] != '\0') {
                    lib::strncpy(out_name, lfn_buf, 64);
                } else {
                    format_8_3_name(ent.name, out_name, 64);
                }

                *out_type = (ent.attributes & FAT_ATTR_DIRECTORY) ? VNodeType::DIRECTORY : VNodeType::FILE;
                *out_size = ent.file_size;
                return 1;
            }

            valid_entry_count++;
            has_lfn = false;
            lfn_buf[0] = '\0';
        }

        curr_cluster = fat32_get_next_cluster(fs, curr_cluster);
    }

    return 0;
}

static VNode* fat32_vnode_lookup(VNode* dir, const char* name) {
    if (!dir || !dir->fs_data || !name) {
        return nullptr;
    }

    auto* data = reinterpret_cast<FatNodeData*>(dir->fs_data);
    Fat32Instance* fs = data->fs;

    uint32_t curr_cluster = data->first_cluster;
    uint8_t cluster_buf[4096];
    char lfn_buf[256];
    lfn_buf[0] = '\0';
    bool has_lfn = false;

    while (curr_cluster >= 2 && curr_cluster < FAT32_CLUSTER_EOC) {
        uint32_t clus_lba = fat32_cluster_to_lba(fs, curr_cluster);
        for (uint32_t s = 0; s < fs->sectors_per_cluster; ++s) {
            if (!drivers::ata_read_sectors(clus_lba + s, 1, cluster_buf + (s * 512))) {
                return nullptr;
            }
        }

        size_t entries_per_cluster = fs->bytes_per_cluster / sizeof(FatDirectoryEntry);
        auto* entries = reinterpret_cast<FatDirectoryEntry*>(cluster_buf);

        for (size_t i = 0; i < entries_per_cluster; ++i) {
            const auto& ent = entries[i];
            uint8_t first_byte = static_cast<uint8_t>(ent.name[0]);

            if (first_byte == 0x00) {
                return nullptr;
            }

            if (first_byte == 0xE5) {
                has_lfn = false;
                lfn_buf[0] = '\0';
                continue;
            }

            if (ent.attributes == FAT_ATTR_LFN) {
                auto* lfn = reinterpret_cast<const FatLfnEntry*>(&ent);
                uint8_t seq = lfn->sequence_number & 0x1F;
                if (seq > 0 && seq <= 20) {
                    size_t char_pos = (seq - 1) * 13;
                    for (int c = 0; c < 5; ++c) {
                        uint16_t ch = lfn->name1[c];
                        if (ch == 0 || ch == 0xFFFF) break;
                        if (char_pos + c < sizeof(lfn_buf) - 1) lfn_buf[char_pos + c] = static_cast<char>(ch);
                    }
                    for (int c = 0; c < 6; ++c) {
                        uint16_t ch = lfn->name2[c];
                        if (ch == 0 || ch == 0xFFFF) break;
                        if (char_pos + 5 + c < sizeof(lfn_buf) - 1) lfn_buf[char_pos + 5 + c] = static_cast<char>(ch);
                    }
                    for (int c = 0; c < 2; ++c) {
                        uint16_t ch = lfn->name3[c];
                        if (ch == 0 || ch == 0xFFFF) break;
                        if (char_pos + 11 + c < sizeof(lfn_buf) - 1) lfn_buf[char_pos + 11 + c] = static_cast<char>(ch);
                    }
                    if (lfn->sequence_number & 0x40) {
                        lfn_buf[char_pos + 13] = '\0';
                    }
                    has_lfn = true;
                }
                continue;
            }

            if (ent.attributes & FAT_ATTR_VOLUME_ID) {
                has_lfn = false;
                lfn_buf[0] = '\0';
                continue;
            }

            char formatted_8_3[64];
            format_8_3_name(ent.name, formatted_8_3, sizeof(formatted_8_3));

            bool matched = false;
            if (has_lfn && lfn_buf[0] != '\0' && str_equals_ignore_case(lfn_buf, name)) {
                matched = true;
            } else if (str_equals_ignore_case(formatted_8_3, name)) {
                matched = true;
            }

            if (matched) {
                auto* node = reinterpret_cast<VNode*>(mm::kmalloc(sizeof(VNode)));
                auto* n_data = reinterpret_cast<FatNodeData*>(mm::kmalloc(sizeof(FatNodeData)));

                lib::strncpy(node->name, name, sizeof(node->name));
                node->type = (ent.attributes & FAT_ATTR_DIRECTORY) ? VNodeType::DIRECTORY : VNodeType::FILE;
                node->size = ent.file_size;
                node->permissions = VFS_PERM_READ | VFS_PERM_WRITE | VFS_PERM_EXEC;
                node->ops = &g_fat32_ops;

                n_data->fs = fs;
                n_data->first_cluster = (static_cast<uint32_t>(ent.cluster_high) << 16) | ent.cluster_low;
                n_data->dir_cluster = curr_cluster;
                n_data->dir_entry_offset = static_cast<uint32_t>(i * sizeof(FatDirectoryEntry));
                n_data->is_directory = (node->type == VNodeType::DIRECTORY);

                node->fs_data = n_data;
                return node;
            }

            has_lfn = false;
            lfn_buf[0] = '\0';
        }

        curr_cluster = fat32_get_next_cluster(fs, curr_cluster);
    }

    return nullptr;
}

static int fat32_vnode_create(VNode* dir, const char* name, VNodeType type) {
    if (!dir || !dir->fs_data || !name) {
        return -1;
    }

    auto* data = reinterpret_cast<FatNodeData*>(dir->fs_data);
    Fat32Instance* fs = data->fs;

    uint32_t new_cluster = fat32_allocate_cluster(fs, 0);
    if (new_cluster == 0) {
        return -1;
    }

    char fat_name[11];
    create_8_3_name(name, fat_name);

    uint32_t curr_cluster = data->first_cluster;
    uint8_t cluster_buf[4096];

    while (curr_cluster >= 2 && curr_cluster < FAT32_CLUSTER_EOC) {
        uint32_t clus_lba = fat32_cluster_to_lba(fs, curr_cluster);
        for (uint32_t s = 0; s < fs->sectors_per_cluster; ++s) {
            drivers::ata_read_sectors(clus_lba + s, 1, cluster_buf + (s * 512));
        }

        size_t entries_per_cluster = fs->bytes_per_cluster / sizeof(FatDirectoryEntry);
        auto* entries = reinterpret_cast<FatDirectoryEntry*>(cluster_buf);

        for (size_t i = 0; i < entries_per_cluster; ++i) {
            uint8_t fb = static_cast<uint8_t>(entries[i].name[0]);
            if (fb == 0x00 || fb == 0xE5) {
                lib::memcpy(entries[i].name, fat_name, 11);
                entries[i].attributes = (type == VNodeType::DIRECTORY) ? FAT_ATTR_DIRECTORY : FAT_ATTR_ARCHIVE;
                entries[i].nt_reserved = 0;
                entries[i].creation_time_tenth = 0;
                entries[i].creation_time = 0;
                entries[i].creation_date = 0;
                entries[i].last_access_date = 0;
                entries[i].cluster_high = static_cast<uint16_t>((new_cluster >> 16) & 0xFFFF);
                entries[i].write_time = 0;
                entries[i].write_date = 0;
                entries[i].cluster_low = static_cast<uint16_t>(new_cluster & 0xFFFF);
                entries[i].file_size = 0;

                for (uint32_t s = 0; s < fs->sectors_per_cluster; ++s) {
                    drivers::ata_write_sectors(clus_lba + s, 1, cluster_buf + (s * 512));
                }

                if (type == VNodeType::DIRECTORY) {
                    uint8_t sub_buf[512];
                    lib::memset(sub_buf, 0, 512);
                    auto* sub_entries = reinterpret_cast<FatDirectoryEntry*>(sub_buf);

                    lib::memcpy(sub_entries[0].name, ".          ", 11);
                    sub_entries[0].attributes = FAT_ATTR_DIRECTORY;
                    sub_entries[0].cluster_high = static_cast<uint16_t>((new_cluster >> 16) & 0xFFFF);
                    sub_entries[0].cluster_low = static_cast<uint16_t>(new_cluster & 0xFFFF);

                    lib::memcpy(sub_entries[1].name, "..         ", 11);
                    sub_entries[1].attributes = FAT_ATTR_DIRECTORY;
                    sub_entries[1].cluster_high = static_cast<uint16_t>((data->first_cluster >> 16) & 0xFFFF);
                    sub_entries[1].cluster_low = static_cast<uint16_t>(data->first_cluster & 0xFFFF);

                    uint32_t sub_lba = fat32_cluster_to_lba(fs, new_cluster);
                    drivers::ata_write_sectors(sub_lba, 1, sub_buf);
                }

                return 0;
            }
        }

        uint32_t next = fat32_get_next_cluster(fs, curr_cluster);
        if (next < 2 || next >= FAT32_CLUSTER_EOC) {
            next = fat32_allocate_cluster(fs, curr_cluster);
            if (next == 0) return -1;
        }
        curr_cluster = next;
    }

    return -1;
}

static int fat32_vnode_remove(VNode* dir, const char* name) {
    if (!dir || !dir->fs_data || !name) {
        return -1;
    }

    auto* data = reinterpret_cast<FatNodeData*>(dir->fs_data);
    Fat32Instance* fs = data->fs;

    uint32_t curr_cluster = data->first_cluster;
    uint8_t cluster_buf[4096];
    char lfn_buf[256];
    lfn_buf[0] = '\0';
    bool has_lfn = false;

    while (curr_cluster >= 2 && curr_cluster < FAT32_CLUSTER_EOC) {
        uint32_t clus_lba = fat32_cluster_to_lba(fs, curr_cluster);
        for (uint32_t s = 0; s < fs->sectors_per_cluster; ++s) {
            if (!drivers::ata_read_sectors(clus_lba + s, 1, cluster_buf + (s * 512))) {
                return -1;
            }
        }

        size_t entries_per_cluster = fs->bytes_per_cluster / sizeof(FatDirectoryEntry);
        auto* entries = reinterpret_cast<FatDirectoryEntry*>(cluster_buf);

        for (size_t i = 0; i < entries_per_cluster; ++i) {
            auto& ent = entries[i];
            uint8_t fb = static_cast<uint8_t>(ent.name[0]);

            if (fb == 0x00) return -1;
            if (fb == 0xE5) {
                has_lfn = false;
                lfn_buf[0] = '\0';
                continue;
            }

            if (ent.attributes == FAT_ATTR_LFN) {
                has_lfn = true;
                continue;
            }

            if (ent.attributes & FAT_ATTR_VOLUME_ID) {
                has_lfn = false;
                continue;
            }

            char formatted_8_3[64];
            format_8_3_name(ent.name, formatted_8_3, sizeof(formatted_8_3));

            bool matched = false;
            if (has_lfn && str_equals_ignore_case(lfn_buf, name)) {
                matched = true;
            } else if (str_equals_ignore_case(formatted_8_3, name)) {
                matched = true;
            }

            if (matched) {
                uint32_t start_clus = (static_cast<uint32_t>(ent.cluster_high) << 16) | ent.cluster_low;
                if (start_clus >= 2) {
                    fat32_free_cluster_chain(fs, start_clus);
                }

                ent.name[0] = static_cast<char>(0xE5);

                for (uint32_t s = 0; s < fs->sectors_per_cluster; ++s) {
                    drivers::ata_write_sectors(clus_lba + s, 1, cluster_buf + (s * 512));
                }

                return 0;
            }

            has_lfn = false;
            lfn_buf[0] = '\0';
        }

        curr_cluster = fat32_get_next_cluster(fs, curr_cluster);
    }

    return -1;
}

VNode* fat32_mount(uint32_t drive_id, uint64_t start_lba, uint64_t sector_count) {
    (void)sector_count;
    if (g_fat32_instance_count >= 4) {
        return nullptr;
    }

    Fat32Instance* fs = &g_fat32_instances[g_fat32_instance_count];
    if (!fat32_probe(drive_id, start_lba, fs)) {
        return nullptr;
    }

    g_fat32_instance_count++;

    auto* root_node = reinterpret_cast<VNode*>(mm::kmalloc(sizeof(VNode)));
    auto* n_data = reinterpret_cast<FatNodeData*>(mm::kmalloc(sizeof(FatNodeData)));

    lib::strncpy(root_node->name, "fat32_root", sizeof(root_node->name));
    root_node->type = VNodeType::DIRECTORY;
    root_node->size = 0;
    root_node->permissions = VFS_PERM_READ | VFS_PERM_WRITE | VFS_PERM_EXEC;
    root_node->ops = &g_fat32_ops;

    n_data->fs = fs;
    n_data->first_cluster = fs->root_cluster;
    n_data->dir_cluster = 0;
    n_data->dir_entry_offset = 0;
    n_data->is_directory = true;

    root_node->fs_data = n_data;
    return root_node;
}

}
