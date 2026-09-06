#include "drivers/pci.hpp"
#include "arch/x86_64/io.hpp"
#include "drivers/serial.hpp"
#include "lib/kprintf.hpp"
#include "lib/string.hpp"

namespace drivers {

static constexpr uint16_t PCI_CONFIG_ADDRESS = 0x0CF8;
static constexpr uint16_t PCI_CONFIG_DATA    = 0x0CFC;
static constexpr size_t   PCI_MAX_DEVICES    = 32;

static PCIDevice g_pci_devices[PCI_MAX_DEVICES];
static size_t    g_pci_device_count = 0;

static uint32_t pci_make_address(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    return (1U << 31)
         | (static_cast<uint32_t>(bus) << 16)
         | (static_cast<uint32_t>(slot) << 11)
         | (static_cast<uint32_t>(func) << 8)
         | (offset & 0xFC);
}

uint32_t pci_read_config_32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t addr = pci_make_address(bus, slot, func, offset);
    arch::outl(PCI_CONFIG_ADDRESS, addr);
    return arch::inl(PCI_CONFIG_DATA);
}

uint16_t pci_read_config_16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t val32 = pci_read_config_32(bus, slot, func, offset);
    return static_cast<uint16_t>((val32 >> ((offset & 2) * 8)) & 0xFFFF);
}

uint8_t pci_read_config_8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t val32 = pci_read_config_32(bus, slot, func, offset);
    return static_cast<uint8_t>((val32 >> ((offset & 3) * 8)) & 0xFF);
}

void pci_write_config_32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val) {
    uint32_t addr = pci_make_address(bus, slot, func, offset);
    arch::outl(PCI_CONFIG_ADDRESS, addr);
    arch::outl(PCI_CONFIG_DATA, val);
}

void pci_write_config_16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t val) {
    uint32_t current = pci_read_config_32(bus, slot, func, offset);
    uint32_t shift = (offset & 2) * 8;
    current &= ~(0xFFFFU << shift);
    current |= (static_cast<uint32_t>(val) << shift);
    pci_write_config_32(bus, slot, func, offset, current);
}

void pci_write_config_8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint8_t val) {
    uint32_t current = pci_read_config_32(bus, slot, func, offset);
    uint32_t shift = (offset & 3) * 8;
    current &= ~(0xFFU << shift);
    current |= (static_cast<uint32_t>(val) << shift);
    pci_write_config_32(bus, slot, func, offset, current);
}

void pci_enable_bus_master(uint8_t bus, uint8_t slot, uint8_t func) {
    uint16_t cmd = pci_read_config_16(bus, slot, func, 0x04);
    cmd |= (1U << 0) | (1U << 1) | (1U << 2);
    pci_write_config_16(bus, slot, func, 0x04, cmd);
}

static void pci_scan_function(uint8_t bus, uint8_t slot, uint8_t func) {
    uint32_t id_reg = pci_read_config_32(bus, slot, func, 0x00);
    uint16_t vendor = static_cast<uint16_t>(id_reg & 0xFFFF);
    uint16_t device = static_cast<uint16_t>((id_reg >> 16) & 0xFFFF);

    if (vendor == 0xFFFF || vendor == 0x0000) {
        return;
    }

    if (g_pci_device_count >= PCI_MAX_DEVICES) {
        return;
    }

    uint32_t class_reg = pci_read_config_32(bus, slot, func, 0x08);
    uint8_t class_code = static_cast<uint8_t>((class_reg >> 24) & 0xFF);
    uint8_t subclass   = static_cast<uint8_t>((class_reg >> 16) & 0xFF);
    uint8_t prog_if    = static_cast<uint8_t>((class_reg >> 8) & 0xFF);

    uint32_t intr_reg = pci_read_config_32(bus, slot, func, 0x3C);
    uint8_t irq_line  = static_cast<uint8_t>(intr_reg & 0xFF);

    uint32_t bar0 = pci_read_config_32(bus, slot, func, 0x10);
    uint32_t bar1 = pci_read_config_32(bus, slot, func, 0x14);
    uint32_t bar2 = pci_read_config_32(bus, slot, func, 0x18);

    PCIDevice& dev = g_pci_devices[g_pci_device_count++];
    dev.bus = bus;
    dev.slot = slot;
    dev.func = func;
    dev.vendor_id = vendor;
    dev.device_id = device;
    dev.class_code = class_code;
    dev.subclass = subclass;
    dev.prog_if = prog_if;
    dev.irq_line = irq_line;
    dev.bar0 = bar0;
    dev.bar1 = bar1;
    dev.bar2 = bar2;
}

void pci_init() {
    g_pci_device_count = 0;

    for (uint16_t bus = 0; bus < 8; ++bus) {
        for (uint8_t slot = 0; slot < 32; ++slot) {
            uint32_t id_reg = pci_read_config_32(static_cast<uint8_t>(bus), slot, 0, 0x00);
            uint16_t vendor = static_cast<uint16_t>(id_reg & 0xFFFF);
            if (vendor == 0xFFFF || vendor == 0x0000) {
                continue;
            }

            pci_scan_function(static_cast<uint8_t>(bus), slot, 0);

            uint8_t header_type = pci_read_config_8(static_cast<uint8_t>(bus), slot, 0, 0x0E);
            if (header_type & 0x80) {
                for (uint8_t func = 1; func < 8; ++func) {
                    pci_scan_function(static_cast<uint8_t>(bus), slot, func);
                }
            }
        }
    }

    drivers::serial_puts("[PCI] Bus enumeration complete. Devices detected: ");
    char num_buf[16];
    size_t cnt = g_pci_device_count;
    size_t idx = 0;
    if (cnt == 0) num_buf[idx++] = '0';
    else {
        char rev[16];
        size_t r = 0;
        while (cnt > 0) { rev[r++] = static_cast<char>('0' + (cnt % 10)); cnt /= 10; }
        while (r > 0) num_buf[idx++] = rev[--r];
    }
    num_buf[idx] = '\0';
    drivers::serial_puts(num_buf);
    drivers::serial_puts("\r\n");
}

bool pci_find_device(uint16_t vendor_id, uint16_t device_id, PCIDevice* out_dev) {
    if (!out_dev) return false;
    for (size_t i = 0; i < g_pci_device_count; ++i) {
        if (g_pci_devices[i].vendor_id == vendor_id && g_pci_devices[i].device_id == device_id) {
            *out_dev = g_pci_devices[i];
            return true;
        }
    }
    return false;
}

bool pci_find_class(uint8_t class_code, uint8_t subclass, PCIDevice* out_dev) {
    if (!out_dev) return false;
    for (size_t i = 0; i < g_pci_device_count; ++i) {
        if (g_pci_devices[i].class_code == class_code && g_pci_devices[i].subclass == subclass) {
            *out_dev = g_pci_devices[i];
            return true;
        }
    }
    return false;
}

size_t pci_get_device_count() {
    return g_pci_device_count;
}

bool pci_get_device(size_t index, PCIDevice* out_dev) {
    if (!out_dev || index >= g_pci_device_count) return false;
    *out_dev = g_pci_devices[index];
    return true;
}

}
