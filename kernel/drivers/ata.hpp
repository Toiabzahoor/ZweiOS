

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

namespace drivers {


inline constexpr uint16_t ATA_PRIMARY_DATA         = 0x1F0;
inline constexpr uint16_t ATA_PRIMARY_ERROR        = 0x1F1;
inline constexpr uint16_t ATA_PRIMARY_SECCOUNT     = 0x1F2;
inline constexpr uint16_t ATA_PRIMARY_LBA_LO       = 0x1F3;
inline constexpr uint16_t ATA_PRIMARY_LBA_MID      = 0x1F4;
inline constexpr uint16_t ATA_PRIMARY_LBA_HI       = 0x1F5;
inline constexpr uint16_t ATA_PRIMARY_DRIVE_HEAD   = 0x1F6;
inline constexpr uint16_t ATA_PRIMARY_STATUS       = 0x1F7;
inline constexpr uint16_t ATA_PRIMARY_COMMAND      = 0x1F7;
inline constexpr uint16_t ATA_PRIMARY_CONTROL      = 0x3F6;


inline constexpr uint8_t ATA_SR_ERR = 0x01;
inline constexpr uint8_t ATA_SR_DRQ = 0x08;
inline constexpr uint8_t ATA_SR_SRV = 0x10;
inline constexpr uint8_t ATA_SR_DF  = 0x20;
inline constexpr uint8_t ATA_SR_RDY = 0x40;
inline constexpr uint8_t ATA_SR_BSY = 0x80;


inline constexpr uint8_t ATA_CMD_READ_PIO   = 0x20;
inline constexpr uint8_t ATA_CMD_WRITE_PIO  = 0x30;
inline constexpr uint8_t ATA_CMD_IDENTIFY   = 0xEC;

void        ata_init(void);
bool        ata_is_available(void);
uint32_t    ata_get_sector_count(void);
const char* ata_get_model(void);
bool        ata_read_sectors(uint32_t lba, uint8_t count, uint8_t* buffer);
bool        ata_write_sectors(uint32_t lba, uint8_t count, const uint8_t* buffer);

}
