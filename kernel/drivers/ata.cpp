

#include "drivers/ata.hpp"
#include "arch/x86_64/io.hpp"
#include "lib/kprintf.hpp"
#include "lib/string.hpp"
#include "drivers/serial.hpp"

namespace drivers {

static bool     g_ata_present = false;
static uint32_t g_ata_sectors = 0;
static char     g_ata_model[41] = {0};

bool ata_is_available(void) {
    return g_ata_present;
}

uint32_t ata_get_sector_count(void) {
    return g_ata_sectors;
}

const char* ata_get_model(void) {
    return g_ata_model;
}

static void ata_delay_400ns(void) {

    inb(ATA_PRIMARY_CONTROL);
    inb(ATA_PRIMARY_CONTROL);
    inb(ATA_PRIMARY_CONTROL);
    inb(ATA_PRIMARY_CONTROL);
}

static bool ata_wait_ready(void) {
    for (int i = 0; i < 100000; ++i) {
        uint8_t status = inb(ATA_PRIMARY_STATUS);
        if (!(status & ATA_SR_BSY)) {
            if (status & (ATA_SR_ERR | ATA_SR_DF)) {
                return false;
            }
            return true;
        }
        io_wait();
    }
    return false;
}

static bool ata_wait_drq(void) {
    for (int i = 0; i < 100000; ++i) {
        uint8_t status = inb(ATA_PRIMARY_STATUS);
        if (status & (ATA_SR_ERR | ATA_SR_DF)) {
            return false;
        }
        if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) {
            return true;
        }
        io_wait();
    }
    return false;
}

void ata_init(void) {
    g_ata_present = false;
    g_ata_sectors = 0;
    lib::memset(g_ata_model, 0, sizeof(g_ata_model));


    outb(ATA_PRIMARY_DRIVE_HEAD, 0xA0);
    ata_delay_400ns();


    outb(ATA_PRIMARY_SECCOUNT, 0);
    outb(ATA_PRIMARY_LBA_LO, 0);
    outb(ATA_PRIMARY_LBA_MID, 0);
    outb(ATA_PRIMARY_LBA_HI, 0);


    outb(ATA_PRIMARY_COMMAND, ATA_CMD_IDENTIFY);
    ata_delay_400ns();

    uint8_t status = inb(ATA_PRIMARY_STATUS);
    if (status == 0) {

        drivers::serial_puts("[ATA] Primary Master: No drive detected.\r\n");
        return;
    }


    uint8_t lba_mid = inb(ATA_PRIMARY_LBA_MID);
    uint8_t lba_hi  = inb(ATA_PRIMARY_LBA_HI);
    if (lba_mid != 0 || lba_hi != 0) {
        drivers::serial_puts("[ATA] Primary Master: Non-ATA device (ATAPI/SATA).\r\n");
        return;
    }

    if (!ata_wait_drq()) {
        drivers::serial_puts("[ATA] Primary Master: Drive not ready or timed out.\r\n");
        return;
    }


    uint16_t id_buf[256];
    for (int i = 0; i < 256; ++i) {
        id_buf[i] = inw(ATA_PRIMARY_DATA);
    }


    size_t char_idx = 0;
    for (int i = 27; i <= 46; ++i) {
        g_ata_model[char_idx++] = static_cast<char>((id_buf[i] >> 8) & 0xFF);
        g_ata_model[char_idx++] = static_cast<char>(id_buf[i] & 0xFF);
    }
    g_ata_model[char_idx] = '\0';


    for (int i = static_cast<int>(char_idx) - 1; i >= 0; --i) {
        if (g_ata_model[i] == ' ') {
            g_ata_model[i] = '\0';
        } else {
            break;
        }
    }


    g_ata_sectors = (static_cast<uint32_t>(id_buf[61]) << 16) | id_buf[60];
    g_ata_present = true;

    lib::kprint_str("[ATA] Primary Master detected: Model '");
    lib::kprint_str(g_ata_model);
    lib::kprint_str("', Total Sectors: ");
    lib::kprint_udec(g_ata_sectors);
    lib::kprint_str(" (");
    lib::kprint_udec((g_ata_sectors * 512) / (1024 * 1024));
    lib::kprint_str(" MB)\r\n");
}

bool ata_read_sectors(uint32_t lba, uint8_t count, uint8_t* buffer) {
    if (!g_ata_present || !buffer) {
        return false;
    }

    for (uint8_t i = 0; i < count; ++i) {
        uint32_t curr_lba = lba + i;

        if (!ata_wait_ready()) return false;


        outb(ATA_PRIMARY_DRIVE_HEAD, 0xE0 | ((curr_lba >> 24) & 0x0F));
        ata_delay_400ns();

        outb(ATA_PRIMARY_SECCOUNT, 1);
        outb(ATA_PRIMARY_LBA_LO, curr_lba & 0xFF);
        outb(ATA_PRIMARY_LBA_MID, (curr_lba >> 8) & 0xFF);
        outb(ATA_PRIMARY_LBA_HI, (curr_lba >> 16) & 0xFF);
        outb(ATA_PRIMARY_COMMAND, ATA_CMD_READ_PIO);

        if (!ata_wait_drq()) return false;

        auto* dest = reinterpret_cast<uint16_t*>(buffer + (i * 512));
        for (int w = 0; w < 256; ++w) {
            dest[w] = inw(ATA_PRIMARY_DATA);
        }
    }

    return true;
}

bool ata_write_sectors(uint32_t lba, uint8_t count, const uint8_t* buffer) {
    if (!g_ata_present || !buffer) {
        return false;
    }

    for (uint8_t i = 0; i < count; ++i) {
        uint32_t curr_lba = lba + i;

        if (!ata_wait_ready()) return false;

        outb(ATA_PRIMARY_DRIVE_HEAD, 0xE0 | ((curr_lba >> 24) & 0x0F));
        ata_delay_400ns();

        outb(ATA_PRIMARY_SECCOUNT, 1);
        outb(ATA_PRIMARY_LBA_LO, curr_lba & 0xFF);
        outb(ATA_PRIMARY_LBA_MID, (curr_lba >> 8) & 0xFF);
        outb(ATA_PRIMARY_LBA_HI, (curr_lba >> 16) & 0xFF);
        outb(ATA_PRIMARY_COMMAND, ATA_CMD_WRITE_PIO);

        if (!ata_wait_drq()) return false;

        const auto* src = reinterpret_cast<const uint16_t*>(buffer + (i * 512));
        for (int w = 0; w < 256; ++w) {
            outw(ATA_PRIMARY_DATA, src[w]);
        }
    }


    outb(ATA_PRIMARY_COMMAND, 0xE7);
    ata_wait_ready();
    return true;
}

}
