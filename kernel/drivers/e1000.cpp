#include "drivers/e1000.hpp"
#include "drivers/pci.hpp"
#include "arch/x86_64/io.hpp"
#include "arch/x86_64/pic.hpp"
#include "arch/x86_64/isr.hpp"
#include "mm/vmm.hpp"
#include "mm/pmm.hpp"
#include "mm/heap.hpp"
#include "drivers/serial.hpp"
#include "lib/kprintf.hpp"
#include "lib/string.hpp"

namespace drivers {

static constexpr uint32_t REG_CTRL     = 0x00000;
static constexpr uint32_t REG_STATUS   = 0x00008;
static constexpr uint32_t REG_EERD     = 0x00014;
static constexpr uint32_t REG_ICR      = 0x000C0;
static constexpr uint32_t REG_ITR      = 0x000C4;
static constexpr uint32_t REG_ICS      = 0x000C8;
static constexpr uint32_t REG_IMS      = 0x000D0;
static constexpr uint32_t REG_IMC      = 0x000D8;
static constexpr uint32_t REG_RCTL     = 0x00100;
static constexpr uint32_t REG_TIPG     = 0x00410;
static constexpr uint32_t REG_RDBAL    = 0x02800;
static constexpr uint32_t REG_RDBAH    = 0x02804;
static constexpr uint32_t REG_RDLEN    = 0x02808;
static constexpr uint32_t REG_RDH      = 0x02810;
static constexpr uint32_t REG_RDT      = 0x02818;
static constexpr uint32_t REG_TCTL     = 0x00400;
static constexpr uint32_t REG_TDBAL    = 0x03800;
static constexpr uint32_t REG_TDBAH    = 0x03804;
static constexpr uint32_t REG_TDLEN    = 0x03808;
static constexpr uint32_t REG_TDH      = 0x03810;
static constexpr uint32_t REG_TDT      = 0x03818;
static constexpr uint32_t REG_MTA      = 0x05200;
static constexpr uint32_t REG_RAL0     = 0x05400;
static constexpr uint32_t REG_RAH0     = 0x05404;

static constexpr uint32_t RCTL_EN       = (1U << 1);
static constexpr uint32_t RCTL_SBP      = (1U << 2);
static constexpr uint32_t RCTL_UPE      = (1U << 3);
static constexpr uint32_t RCTL_MPE      = (1U << 4);
static constexpr uint32_t RCTL_LPE      = (1U << 5);
static constexpr uint32_t RCTL_BAM      = (1U << 15);
static constexpr uint32_t RCTL_SECRC    = (1U << 26);
static constexpr uint32_t RCTL_BSIZE_2K = (0U << 16);

static constexpr uint32_t TCTL_EN       = (1U << 1);
static constexpr uint32_t TCTL_PSP      = (1U << 3);
static constexpr uint32_t TCTL_CT_SHIFT = 4;
static constexpr uint32_t TCTL_COLD_SH  = 12;

static constexpr uint8_t  TXD_CMD_EOP   = (1U << 0);
static constexpr uint8_t  TXD_CMD_IFCS  = (1U << 1);
static constexpr uint8_t  TXD_CMD_RS    = (1U << 3);
static constexpr uint8_t  TXD_STAT_DD   = (1U << 0);

static constexpr uint8_t  RXD_STAT_DD   = (1U << 0);
static constexpr uint8_t  RXD_STAT_EOP  = (1U << 1);

static constexpr size_t NUM_RX_DESC = 64;
static constexpr size_t NUM_TX_DESC = 64;
static constexpr size_t PKT_BUF_SZ  = 2048;

static bool      g_e1000_available = false;
static uint64_t  g_mmio_base = 0;
static uint8_t   g_mac_address[6] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
static uint8_t   g_irq_line = 11;

alignas(4096) static E1000RxDesc g_rx_ring[NUM_RX_DESC];
alignas(4096) static E1000TxDesc g_tx_ring[NUM_TX_DESC];

static uint8_t g_rx_buffers[NUM_RX_DESC][PKT_BUF_SZ];
static uint8_t g_tx_buffers[NUM_TX_DESC][PKT_BUF_SZ];

static uint16_t g_rx_cur = 0;
static uint16_t g_tx_cur = 0;

static uint64_t g_rx_packets = 0;
static uint64_t g_tx_packets = 0;
static uint64_t g_rx_bytes   = 0;
static uint64_t g_tx_bytes   = 0;

static inline void e1000_write32(uint32_t reg, uint32_t val) {
    *reinterpret_cast<volatile uint32_t*>(g_mmio_base + reg) = val;
}

static inline uint32_t e1000_read32(uint32_t reg) {
    return *reinterpret_cast<volatile uint32_t*>(g_mmio_base + reg);
}

static uint16_t e1000_read_eeprom(uint8_t addr) {
    uint32_t val = 0;
    e1000_write32(REG_EERD, (static_cast<uint32_t>(addr) << 8) | 1);
    for (size_t i = 0; i < 10000; ++i) {
        val = e1000_read32(REG_EERD);
        if (val & (1U << 4)) {
            break;
        }
    }
    return static_cast<uint16_t>((val >> 16) & 0xFFFF);
}

static void e1000_read_mac() {
    uint32_t ral = e1000_read32(REG_RAL0);
    uint32_t rah = e1000_read32(REG_RAH0);

    if (ral != 0 && ral != 0xFFFFFFFF) {
        g_mac_address[0] = static_cast<uint8_t>(ral & 0xFF);
        g_mac_address[1] = static_cast<uint8_t>((ral >> 8) & 0xFF);
        g_mac_address[2] = static_cast<uint8_t>((ral >> 16) & 0xFF);
        g_mac_address[3] = static_cast<uint8_t>((ral >> 24) & 0xFF);
        g_mac_address[4] = static_cast<uint8_t>(rah & 0xFF);
        g_mac_address[5] = static_cast<uint8_t>((rah >> 8) & 0xFF);
        return;
    }

    uint16_t val0 = e1000_read_eeprom(0);
    uint16_t val1 = e1000_read_eeprom(1);
    uint16_t val2 = e1000_read_eeprom(2);

    if (val0 != 0 && val0 != 0xFFFF) {
        g_mac_address[0] = static_cast<uint8_t>(val0 & 0xFF);
        g_mac_address[1] = static_cast<uint8_t>((val0 >> 8) & 0xFF);
        g_mac_address[2] = static_cast<uint8_t>(val1 & 0xFF);
        g_mac_address[3] = static_cast<uint8_t>((val1 >> 8) & 0xFF);
        g_mac_address[4] = static_cast<uint8_t>(val2 & 0xFF);
        g_mac_address[5] = static_cast<uint8_t>((val2 >> 8) & 0xFF);
    }
}

static void e1000_rx_init() {
    uint64_t ring_phys = mm::vmm_virt_to_phys(reinterpret_cast<uint64_t>(g_rx_ring));

    for (size_t i = 0; i < NUM_RX_DESC; ++i) {
        uint64_t buf_phys = mm::vmm_virt_to_phys(reinterpret_cast<uint64_t>(g_rx_buffers[i]));
        g_rx_ring[i].buffer_addr = buf_phys;
        g_rx_ring[i].status = 0;
        g_rx_ring[i].errors = 0;
        g_rx_ring[i].length = 0;
        g_rx_ring[i].checksum = 0;
        g_rx_ring[i].special = 0;
    }

    e1000_write32(REG_RDBAL, static_cast<uint32_t>(ring_phys & 0xFFFFFFFF));
    e1000_write32(REG_RDBAH, static_cast<uint32_t>(ring_phys >> 32));
    e1000_write32(REG_RDLEN, static_cast<uint32_t>(NUM_RX_DESC * sizeof(E1000RxDesc)));

    e1000_write32(REG_RDH, 0);
    e1000_write32(REG_RDT, static_cast<uint32_t>(NUM_RX_DESC - 1));

    uint32_t rctl = RCTL_EN | RCTL_BAM | RCTL_MPE | RCTL_SECRC | RCTL_BSIZE_2K;
    e1000_write32(REG_RCTL, rctl);
    g_rx_cur = 0;
}

static void e1000_tx_init() {
    uint64_t ring_phys = mm::vmm_virt_to_phys(reinterpret_cast<uint64_t>(g_tx_ring));

    for (size_t i = 0; i < NUM_TX_DESC; ++i) {
        uint64_t buf_phys = mm::vmm_virt_to_phys(reinterpret_cast<uint64_t>(g_tx_buffers[i]));
        g_tx_ring[i].buffer_addr = buf_phys;
        g_tx_ring[i].cmd = 0;
        g_tx_ring[i].status = TXD_STAT_DD;
        g_tx_ring[i].length = 0;
        g_tx_ring[i].cso = 0;
        g_tx_ring[i].css = 0;
        g_tx_ring[i].special = 0;
    }

    e1000_write32(REG_TDBAL, static_cast<uint32_t>(ring_phys & 0xFFFFFFFF));
    e1000_write32(REG_TDBAH, static_cast<uint32_t>(ring_phys >> 32));
    e1000_write32(REG_TDLEN, static_cast<uint32_t>(NUM_TX_DESC * sizeof(E1000TxDesc)));

    e1000_write32(REG_TDH, 0);
    e1000_write32(REG_TDT, 0);

    e1000_write32(REG_TIPG, 10 | (10 << 10) | (10 << 20));

    uint32_t tctl = TCTL_EN | TCTL_PSP | (0x0FU << TCTL_CT_SHIFT) | (0x40U << TCTL_COLD_SH);
    e1000_write32(REG_TCTL, tctl);
    g_tx_cur = 0;
}

bool e1000_init() {
    PCIDevice dev;
    bool found = false;

    const uint16_t supported_devs[] = { 0x100E, 0x1004, 0x100F, 0x107D };
    for (size_t i = 0; i < sizeof(supported_devs)/sizeof(supported_devs[0]); ++i) {
        if (pci_find_device(0x8086, supported_devs[i], &dev)) {
            found = true;
            break;
        }
    }

    if (!found) {
        if (pci_find_class(0x02, 0x00, &dev)) {
            if (dev.vendor_id == 0x8086) {
                found = true;
            }
        }
    }

    if (!found) {
        drivers::serial_puts("[E1000] No compatible Intel Gigabit Ethernet controller found.\r\n");
        return false;
    }

    pci_enable_bus_master(dev.bus, dev.slot, dev.func);

    uint64_t paddr = dev.bar0 & 0xFFFFFFF0ULL;
    if (paddr == 0) {
        drivers::serial_puts("[E1000] Invalid BAR0 physical memory address.\r\n");
        return false;
    }

    uint64_t vaddr = 0xFFFFFFFFD8000000ULL;
    for (uint64_t p = 0; p < (128 * 1024); p += mm::PAGE_SIZE) {
        mm::vmm_map_page(vaddr + p, paddr + p, mm::PTE_PRESENT | mm::PTE_WRITABLE | mm::PTE_PCD);
    }
    g_mmio_base = vaddr;

    e1000_read32(REG_ICR);

    e1000_write32(REG_CTRL, e1000_read32(REG_CTRL) | (1U << 26));
    for (volatile size_t delay = 0; delay < 100000; ++delay);

    e1000_write32(REG_CTRL, (1U << 6));

    for (size_t i = 0; i < 128; ++i) {
        e1000_write32(REG_MTA + static_cast<uint32_t>(i * 4), 0);
    }

    e1000_read_mac();

    e1000_rx_init();
    e1000_tx_init();

    e1000_write32(REG_IMS, (1U << 7) | (1U << 6) | (1U << 4) | (1U << 2) | (1U << 1) | (1U << 0));
    e1000_read32(REG_ICR);

    uint8_t irq = dev.irq_line;
    if (irq > 0 && irq < 16) {
        g_irq_line = irq;
        uint8_t isr_num = static_cast<uint8_t>(32 + irq);
        arch::isr_register_handler(isr_num, [](arch::cpu_registers_t*) {
            e1000_handle_irq();
        });
        arch::pic_clear_mask(irq);
    }

    g_e1000_available = true;

    drivers::serial_puts("[E1000] Intel 82540EM Gigabit NIC initialized. MAC: ");
    const char hex_chars[] = "0123456789ABCDEF";
    for (size_t i = 0; i < 6; ++i) {
        if (i > 0) drivers::serial_putc(':');
        drivers::serial_putc(hex_chars[(g_mac_address[i] >> 4) & 0xF]);
        drivers::serial_putc(hex_chars[g_mac_address[i] & 0xF]);
    }
    drivers::serial_puts("\r\n");

    return true;
}

bool e1000_is_available() {
    return g_e1000_available;
}

void e1000_get_mac(uint8_t out_mac[6]) {
    if (!out_mac) return;
    for (size_t i = 0; i < 6; ++i) {
        out_mac[i] = g_mac_address[i];
    }
}

bool e1000_send_packet(const uint8_t* data, size_t len) {
    if (!g_e1000_available || !data || len == 0 || len > PKT_BUF_SZ) {
        return false;
    }

    uint16_t idx = g_tx_cur;
    volatile E1000TxDesc* desc = &g_tx_ring[idx];

    for (size_t retry = 0; retry < 10000; ++retry) {
        if (desc->status & TXD_STAT_DD) break;
    }

    lib::memcpy(g_tx_buffers[idx], data, len);

    desc->length = static_cast<uint16_t>(len);
    desc->cmd = TXD_CMD_EOP | TXD_CMD_IFCS | TXD_CMD_RS;
    desc->status = 0;

    g_tx_cur = static_cast<uint16_t>((idx + 1) % NUM_TX_DESC);
    e1000_write32(REG_TDT, g_tx_cur);

    g_tx_packets++;
    g_tx_bytes += len;
    return true;
}

size_t e1000_receive_packet(uint8_t* out_buf, size_t max_len) {
    if (!g_e1000_available || !out_buf || max_len == 0) {
        return 0;
    }

    uint16_t idx = g_rx_cur;
    volatile E1000RxDesc* desc = &g_rx_ring[idx];

    if (!(desc->status & RXD_STAT_DD)) {
        return 0;
    }

    size_t len = static_cast<size_t>(desc->length);
    size_t copy_len = (len < max_len) ? len : max_len;

    lib::memcpy(out_buf, g_rx_buffers[idx], copy_len);

    desc->status = 0;
    e1000_write32(REG_RDT, idx);

    g_rx_cur = static_cast<uint16_t>((idx + 1) % NUM_RX_DESC);
    g_rx_packets++;
    g_rx_bytes += len;
    return copy_len;
}

void e1000_handle_irq() {
    if (!g_e1000_available) return;
    uint32_t icr = e1000_read32(REG_ICR);
    (void)icr;
    arch::pic_send_eoi(g_irq_line);
}

uint64_t e1000_get_rx_packets() { return g_rx_packets; }
uint64_t e1000_get_tx_packets() { return g_tx_packets; }
uint64_t e1000_get_rx_bytes()   { return g_rx_bytes; }
uint64_t e1000_get_tx_bytes()   { return g_tx_bytes; }

}
