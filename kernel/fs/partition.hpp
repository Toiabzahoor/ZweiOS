#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

namespace fs {

inline constexpr size_t MAX_PARTITIONS = 16;

enum class PartitionScheme {
    NONE = 0,
    MBR,
    GPT
};

enum class FilesystemType {
    UNKNOWN = 0,
    FAT12,
    FAT16,
    FAT32,
    EXT2,
    NTFS,
    RAW
};

struct __attribute__((packed)) MbrPartitionRecord {
    uint8_t  bootable;
    uint8_t  start_head;
    uint16_t start_sector_cyl;
    uint8_t  type;
    uint8_t  end_head;
    uint16_t end_sector_cyl;
    uint32_t start_lba;
    uint32_t sector_count;
};

struct __attribute__((packed)) MbrSector {
    uint8_t            bootstrap[446];
    MbrPartitionRecord entries[4];
    uint16_t           signature;
};

struct __attribute__((packed)) GptHeader {
    uint64_t signature;
    uint32_t revision;
    uint32_t header_size;
    uint32_t header_crc32;
    uint32_t reserved;
    uint64_t current_lba;
    uint64_t backup_lba;
    uint64_t first_usable_lba;
    uint64_t last_usable_lba;
    uint8_t  disk_guid[16];
    uint64_t partition_entries_lba;
    uint32_t num_partition_entries;
    uint32_t partition_entry_size;
    uint32_t partition_entries_crc32;
};

struct __attribute__((packed)) GptEntry {
    uint8_t  type_guid[16];
    uint8_t  unique_guid[16];
    uint64_t start_lba;
    uint64_t end_lba;
    uint64_t attributes;
    uint16_t name[36];
};

struct PartitionInfo {
    uint32_t        drive_id;
    uint32_t        part_index;
    PartitionScheme scheme;
    FilesystemType  fs_type;
    uint64_t        start_lba;
    uint64_t        sector_count;
    char            label[64];
    bool            is_bootable;
};

void            partition_init(void);
bool            partition_probe(uint32_t drive_id);
size_t          partition_get_count(void);
bool            partition_get(size_t index, PartitionInfo* out_info);
const char*     partition_fs_type_to_string(FilesystemType type);
FilesystemType  partition_detect_filesystem(uint32_t drive_id, uint64_t start_lba);

}
