#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "fs/vfs.hpp"

namespace fs {

inline constexpr uint8_t FAT_ATTR_READ_ONLY = 0x01;
inline constexpr uint8_t FAT_ATTR_HIDDEN    = 0x02;
inline constexpr uint8_t FAT_ATTR_SYSTEM    = 0x04;
inline constexpr uint8_t FAT_ATTR_VOLUME_ID = 0x08;
inline constexpr uint8_t FAT_ATTR_DIRECTORY = 0x10;
inline constexpr uint8_t FAT_ATTR_ARCHIVE   = 0x20;
inline constexpr uint8_t FAT_ATTR_LFN       = 0x0F;

inline constexpr uint32_t FAT32_CLUSTER_FREE = 0x00000000;
inline constexpr uint32_t FAT32_CLUSTER_BAD  = 0x0FFFFFF7;
inline constexpr uint32_t FAT32_CLUSTER_EOC  = 0x0FFFFFF8;

struct __attribute__((packed)) Fat32BootSector {
    uint8_t  jmp_boot[3];
    char     oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fat_count;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t head_count;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint32_t fat_size_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info_sector;
    uint16_t backup_boot_sector;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_signature;
    uint32_t volume_id;
    char     volume_label[11];
    char     fs_type_label[8];
    uint8_t  boot_code[420];
    uint16_t signature;
};

struct __attribute__((packed)) FatDirectoryEntry {
    char     name[11];
    uint8_t  attributes;
    uint8_t  nt_reserved;
    uint8_t  creation_time_tenth;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t cluster_high;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t cluster_low;
    uint32_t file_size;
};

struct __attribute__((packed)) FatLfnEntry {
    uint8_t  sequence_number;
    uint16_t name1[5];
    uint8_t  attributes;
    uint8_t  type;
    uint8_t  checksum;
    uint16_t name2[6];
    uint16_t first_cluster;
    uint16_t name3[2];
};

struct Fat32Instance {
    uint32_t drive_id;
    uint64_t partition_start_lba;
    uint32_t bytes_per_sector;
    uint32_t sectors_per_cluster;
    uint32_t bytes_per_cluster;
    uint32_t reserved_sectors;
    uint32_t fat_count;
    uint32_t fat_size_sectors;
    uint32_t fat_start_lba;
    uint32_t data_start_lba;
    uint32_t root_cluster;
    uint32_t total_clusters;
    char     volume_label[12];
    bool     mounted;
};

struct FatNodeData {
    Fat32Instance* fs;
    uint32_t       first_cluster;
    uint32_t       dir_cluster;
    uint32_t       dir_entry_offset;
    bool           is_directory;
};

void   fat32_init(void);
VNode* fat32_mount(uint32_t drive_id, uint64_t start_lba, uint64_t sector_count);
bool   fat32_probe(uint32_t drive_id, uint64_t start_lba, Fat32Instance* out_fs);

}
