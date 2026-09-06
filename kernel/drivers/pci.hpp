#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

namespace drivers {

struct PCIDevice {
    uint8_t  bus;
    uint8_t  slot;
    uint8_t  func;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  class_code;
    uint8_t  subclass;
    uint8_t  prog_if;
    uint8_t  irq_line;
    uint32_t bar0;
    uint32_t bar1;
    uint32_t bar2;
};

void     pci_init();
uint32_t pci_read_config_32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint16_t pci_read_config_16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint8_t  pci_read_config_8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);

void     pci_write_config_32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val);
void     pci_write_config_16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t val);
void     pci_write_config_8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint8_t val);

void     pci_enable_bus_master(uint8_t bus, uint8_t slot, uint8_t func);
bool     pci_find_device(uint16_t vendor_id, uint16_t device_id, PCIDevice* out_dev);
bool     pci_find_class(uint8_t class_code, uint8_t subclass, PCIDevice* out_dev);
size_t   pci_get_device_count();
bool     pci_get_device(size_t index, PCIDevice* out_dev);

}
