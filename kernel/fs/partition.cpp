#include "fs/partition.hpp"
#include "drivers/ata.hpp"
#include "lib/string.hpp"
#include "lib/kprintf.hpp"

namespace fs {

static PartitionInfo g_partitions[MAX_PARTITIONS];
static size_t        g_partition_count = 0;

void partition_init(void) {
    g_partition_count = 0;
    for (size_t i = 0; i < MAX_PARTITIONS; ++i) {
        g_partitions[i].drive_id = 0;
        g_partitions[i].part_index = 0;
        g_partitions[i].scheme = PartitionScheme::NONE;
        g_partitions[i].fs_type = FilesystemType::UNKNOWN;
        g_partitions[i].start_lba = 0;
        g_partitions[i].sector_count = 0;
        g_partitions[i].label[0] = '\0';
        g_partitions[i].is_bootable = false;
    }
}

size_t partition_get_count(void) {
    return g_partition_count;
}

bool partition_get(size_t index, PartitionInfo* out_info) {
    if (!out_info || index >= g_partition_count) {
        return false;
    }
    *out_info = g_partitions[index];
    return true;
}

const char* partition_fs_type_to_string(FilesystemType type) {
    switch (type) {
        case FilesystemType::FAT12:   return "FAT12";
        case FilesystemType::FAT16:   return "FAT16";
        case FilesystemType::FAT32:   return "FAT32";
        case FilesystemType::EXT2:    return "EXT2/4";
        case FilesystemType::NTFS:    return "NTFS/exFAT";
        case FilesystemType::RAW:     return "RAW";
        case FilesystemType::UNKNOWN:
        default:                      return "UNKNOWN";
    }
}

FilesystemType partition_detect_filesystem(uint32_t drive_id, uint64_t start_lba) {
    (void)drive_id;
    uint8_t sector[512];
    if (!drivers::ata_read_sectors(static_cast<uint32_t>(start_lba), 1, sector)) {
        return FilesystemType::UNKNOWN;
    }

    if (sector[510] != 0x55 || sector[511] != 0xAA) {
        return FilesystemType::RAW;
    }

    if (lib::memcmp(&sector[0x52], "FAT32", 5) == 0) {
        return FilesystemType::FAT32;
    }

    if (lib::memcmp(&sector[0x36], "FAT16", 5) == 0) {
        return FilesystemType::FAT16;
    }

    if (lib::memcmp(&sector[0x36], "FAT12", 5) == 0) {
        return FilesystemType::FAT12;
    }

    if (lib::memcmp(&sector[0x03], "NTFS", 4) == 0 || lib::memcmp(&sector[0x03], "EXFAT", 5) == 0) {
        return FilesystemType::NTFS;
    }

    uint16_t bytes_per_sec = *reinterpret_cast<uint16_t*>(&sector[11]);
    uint8_t  sec_per_clus  = sector[13];
    uint32_t fat_sz32      = *reinterpret_cast<uint32_t*>(&sector[36]);

    if (bytes_per_sec == 512 && sec_per_clus > 0 && fat_sz32 > 0) {
        return FilesystemType::FAT32;
    }

    return FilesystemType::UNKNOWN;
}

static bool probe_gpt(uint32_t drive_id) {
    uint8_t sector[512];
    if (!drivers::ata_read_sectors(1, 1, sector)) {
        return false;
    }

    auto* gpt = reinterpret_cast<const GptHeader*>(sector);
    if (gpt->signature != 0x5452415020494645ULL) {
        return false;
    }

    uint64_t entries_lba = gpt->partition_entries_lba;
    uint32_t num_entries = gpt->num_partition_entries;
    uint32_t entry_size  = gpt->partition_entry_size;

    if (entry_size == 0 || num_entries == 0) {
        return false;
    }

    uint32_t sectors_to_read = (num_entries * entry_size + 511) / 512;
    if (sectors_to_read > 32) sectors_to_read = 32;

    uint8_t entries_buf[512];
    for (uint32_t s = 0; s < sectors_to_read; ++s) {
        if (!drivers::ata_read_sectors(static_cast<uint32_t>(entries_lba + s), 1, entries_buf)) {
            break;
        }

        uint32_t entries_per_sec = 512 / entry_size;
        for (uint32_t e = 0; e < entries_per_sec; ++e) {
            auto* entry = reinterpret_cast<const GptEntry*>(entries_buf + (e * entry_size));

            bool is_empty = true;
            for (size_t g = 0; g < 16; ++g) {
                if (entry->type_guid[g] != 0) {
                    is_empty = false;
                    break;
                }
            }

            if (is_empty || entry->start_lba == 0 || entry->end_lba < entry->start_lba) {
                continue;
            }

            if (g_partition_count >= MAX_PARTITIONS) {
                return true;
            }

            PartitionInfo& info = g_partitions[g_partition_count];
            info.drive_id = drive_id;
            info.part_index = static_cast<uint32_t>(g_partition_count);
            info.scheme = PartitionScheme::GPT;
            info.start_lba = entry->start_lba;
            info.sector_count = (entry->end_lba - entry->start_lba) + 1;
            info.is_bootable = false;

            size_t n_len = 0;
            for (size_t c = 0; c < 36; ++c) {
                uint16_t ch = entry->name[c];
                if (ch == 0) break;
                info.label[n_len++] = (ch < 128) ? static_cast<char>(ch) : '?';
            }
            info.label[n_len] = '\0';
            if (n_len == 0) {
                lib::strncpy(info.label, "GPT_Data", sizeof(info.label));
            }

            info.fs_type = partition_detect_filesystem(drive_id, info.start_lba);

            g_partition_count++;
        }
    }

    return (g_partition_count > 0);
}

bool partition_probe(uint32_t drive_id) {
    if (!drivers::ata_is_available()) {
        return false;
    }

    uint8_t sector0[512];
    if (!drivers::ata_read_sectors(0, 1, sector0)) {
        return false;
    }

    if (sector0[510] != 0x55 || sector0[511] != 0xAA) {
        return false;
    }

    auto* mbr = reinterpret_cast<const MbrSector*>(sector0);

    bool has_gpt_protective = false;
    for (int i = 0; i < 4; ++i) {
        if (mbr->entries[i].type == 0xEE) {
            has_gpt_protective = true;
            break;
        }
    }

    if (has_gpt_protective && probe_gpt(drive_id)) {
        return true;
    }

    uint8_t sec1[512];
    if (drivers::ata_read_sectors(1, 1, sec1)) {
        auto* gpt_hdr = reinterpret_cast<const GptHeader*>(sec1);
        if (gpt_hdr->signature == 0x5452415020494645ULL) {
            if (probe_gpt(drive_id)) {
                return true;
            }
        }
    }

    bool found_mbr_entry = false;
    for (int i = 0; i < 4; ++i) {
        const auto& ent = mbr->entries[i];
        if (ent.sector_count == 0 || ent.type == 0) {
            continue;
        }

        if (g_partition_count >= MAX_PARTITIONS) {
            break;
        }

        found_mbr_entry = true;
        PartitionInfo& info = g_partitions[g_partition_count];
        info.drive_id = drive_id;
        info.part_index = static_cast<uint32_t>(g_partition_count);
        info.scheme = PartitionScheme::MBR;
        info.start_lba = ent.start_lba;
        info.sector_count = ent.sector_count;
        info.is_bootable = (ent.bootable == 0x80);

        if (ent.type == 0x0B || ent.type == 0x0C) {
            info.fs_type = FilesystemType::FAT32;
            lib::strncpy(info.label, "FAT32", sizeof(info.label));
        } else if (ent.type == 0x04 || ent.type == 0x06 || ent.type == 0x0E) {
            info.fs_type = FilesystemType::FAT16;
            lib::strncpy(info.label, "FAT16", sizeof(info.label));
        } else if (ent.type == 0x01) {
            info.fs_type = FilesystemType::FAT12;
            lib::strncpy(info.label, "FAT12", sizeof(info.label));
        } else if (ent.type == 0x07) {
            info.fs_type = FilesystemType::NTFS;
            lib::strncpy(info.label, "NTFS", sizeof(info.label));
        } else if (ent.type == 0x83) {
            info.fs_type = FilesystemType::EXT2;
            lib::strncpy(info.label, "LINUX", sizeof(info.label));
        } else {
            info.fs_type = partition_detect_filesystem(drive_id, info.start_lba);
            lib::strncpy(info.label, "PARTITION", sizeof(info.label));
        }

        g_partition_count++;
    }

    if (!found_mbr_entry) {
        FilesystemType raw_fs = partition_detect_filesystem(drive_id, 0);
        if (raw_fs != FilesystemType::UNKNOWN) {
            if (g_partition_count < MAX_PARTITIONS) {
                PartitionInfo& info = g_partitions[g_partition_count];
                info.drive_id = drive_id;
                info.part_index = static_cast<uint32_t>(g_partition_count);
                info.scheme = PartitionScheme::NONE;
                info.fs_type = raw_fs;
                info.start_lba = 0;
                info.sector_count = drivers::ata_get_sector_count();
                info.is_bootable = false;
                lib::strncpy(info.label, partition_fs_type_to_string(raw_fs), sizeof(info.label));
                g_partition_count++;
                return true;
            }
        }
    }

    return (g_partition_count > 0);
}

}
