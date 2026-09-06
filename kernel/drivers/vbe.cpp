#include "drivers/vbe.hpp"
#include "arch/x86_64/io.hpp"
#include "mm/heap.hpp"
#include "mm/vmm.hpp"
#include "lib/string.hpp"
#include "lib/kprintf.hpp"

extern "C" uint64_t pd_table_2[];

namespace drivers {

static FramebufferInfo g_fb_info;
static bool            g_vbe_present = false;
static uint64_t        g_lfb_paddr = VBE_LFB_PHYSICAL_ADDR;

static void vbe_write(uint16_t index, uint16_t data) {
    outw(VBE_DISPI_IOPORT_INDEX, index);
    outw(VBE_DISPI_IOPORT_DATA, data);
}

static uint16_t vbe_read(uint16_t index) {
    outw(VBE_DISPI_IOPORT_INDEX, index);
    return inw(VBE_DISPI_IOPORT_DATA);
}

static uint32_t pci_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (1U << 31)
                     | (static_cast<uint32_t>(bus) << 16)
                     | (static_cast<uint32_t>(slot) << 11)
                     | (static_cast<uint32_t>(func) << 8)
                     | (offset & 0xFC);
    arch::outl(0x0CF8, address);
    return arch::inl(0x0CFC);
}

uint64_t vbe_find_lfb_paddr(void) {
    for (uint16_t bus = 0; bus < 8; ++bus) {
        for (uint8_t slot = 0; slot < 32; ++slot) {
            for (uint8_t func = 0; func < 8; ++func) {
                uint32_t dev_ven = pci_read_dword(static_cast<uint8_t>(bus), slot, func, 0x00);
                uint16_t vendor = static_cast<uint16_t>(dev_ven & 0xFFFF);
                uint16_t device = static_cast<uint16_t>(dev_ven >> 16);
                if (vendor == 0xFFFF) {
                    if (func == 0) break;
                    continue;
                }
                uint32_t class_reg = pci_read_dword(static_cast<uint8_t>(bus), slot, func, 0x08);
                uint8_t base_class = static_cast<uint8_t>(class_reg >> 24);
                if (base_class == 0x03 || (vendor == 0x1234 && device == 0x1111)) {
                    uint32_t bar0 = pci_read_dword(static_cast<uint8_t>(bus), slot, func, 0x10);
                    uint64_t paddr = bar0 & 0xFFFFFFF0;
                    if (paddr != 0) {
                        return paddr;
                    }
                }
            }
        }
    }
    return VBE_LFB_PHYSICAL_ADDR;
}

void vbe_init(void) {
    g_fb_info.width = 0;
    g_fb_info.height = 0;
    g_fb_info.pitch = 0;
    g_fb_info.bpp = 0;
    g_fb_info.bytes_per_pixel = 0;
    g_fb_info.back_buffer = nullptr;
    g_fb_info.front_buffer = nullptr;
    g_fb_info.is_active = false;

    uint16_t id = vbe_read(VBE_DISPI_INDEX_ID);
    if (id >= 0xB0C0 && id <= 0xB0C6) {
        g_vbe_present = true;
    } else {
        g_vbe_present = false;
    }
}

bool vbe_is_available(void) {
    return g_vbe_present;
}

bool vbe_set_mode(uint32_t width, uint32_t height, uint32_t bpp) {
    if (!g_vbe_present) {
        return false;
    }

    g_lfb_paddr = vbe_find_lfb_paddr();

    vbe_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    vbe_write(VBE_DISPI_INDEX_XRES, static_cast<uint16_t>(width));
    vbe_write(VBE_DISPI_INDEX_YRES, static_cast<uint16_t>(height));
    vbe_write(VBE_DISPI_INDEX_BPP, static_cast<uint16_t>(bpp));
    vbe_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED | VBE_DISPI_NOCLEARMEM);

    uint32_t pitch = width * (bpp / 8);
    size_t fb_size = pitch * height;

    uint64_t* pd2 = reinterpret_cast<uint64_t*>(0xFFFFFFFF80000000ULL + reinterpret_cast<uint64_t>(pd_table_2));
    for (size_t i = 0; i < 32; ++i) {
        pd2[256 + i] = (g_lfb_paddr + (i * 0x200000ULL)) | 0x83;
    }
    for (size_t i = 0; i < 32; ++i) {
        mm::vmm_flush_tlb(VBE_LFB_VIRTUAL_ADDR + (i * 0x200000ULL));
    }

    if (!g_fb_info.back_buffer) {
        g_fb_info.back_buffer = reinterpret_cast<uint32_t*>(mm::kmalloc(fb_size));
    }

    g_fb_info.front_buffer = reinterpret_cast<uint32_t*>(VBE_LFB_VIRTUAL_ADDR);
    g_fb_info.width = width;
    g_fb_info.height = height;
    g_fb_info.pitch = pitch;
    g_fb_info.bpp = bpp;
    g_fb_info.bytes_per_pixel = bpp / 8;
    g_fb_info.is_active = true;

    if (g_fb_info.back_buffer) {
        lib::memset(g_fb_info.back_buffer, 0, fb_size);
    }

    return true;
}

void vbe_set_mode_disabled(void) {
    if (!g_vbe_present) return;
    vbe_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    g_fb_info.is_active = false;
}

FramebufferInfo* vbe_get_info(void) {
    return &g_fb_info;
}

void vbe_swap_buffers(void) {
    if (!g_fb_info.is_active || !g_fb_info.back_buffer || !g_fb_info.front_buffer) {
        return;
    }

    size_t total_dwords = (g_fb_info.width * g_fb_info.height);
    lib::memcpy(g_fb_info.front_buffer, g_fb_info.back_buffer, total_dwords * 4);
}

void vbe_swap_rect(int x, int y, int w, int h) {
    if (!g_fb_info.is_active || !g_fb_info.back_buffer || !g_fb_info.front_buffer) {
        return;
    }

    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > static_cast<int>(g_fb_info.width)) w = static_cast<int>(g_fb_info.width) - x;
    if (y + h > static_cast<int>(g_fb_info.height)) h = static_cast<int>(g_fb_info.height) - y;
    if (w <= 0 || h <= 0) return;

    for (int row = y; row < y + h; ++row) {
        size_t offset = (static_cast<size_t>(row) * g_fb_info.width) + static_cast<size_t>(x);
        lib::memcpy(&g_fb_info.front_buffer[offset], &g_fb_info.back_buffer[offset], static_cast<size_t>(w) * 4);
    }
}

void vbe_clear(uint32_t color) {
    if (!g_fb_info.is_active || !g_fb_info.back_buffer) {
        return;
    }

    size_t total_pixels = g_fb_info.width * g_fb_info.height;
    for (size_t i = 0; i < total_pixels; ++i) {
        g_fb_info.back_buffer[i] = color;
    }
}

}
