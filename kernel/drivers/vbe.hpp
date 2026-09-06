#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

namespace drivers {

inline constexpr uint16_t VBE_DISPI_IOPORT_INDEX = 0x01CE;
inline constexpr uint16_t VBE_DISPI_IOPORT_DATA  = 0x01CF;

inline constexpr uint16_t VBE_DISPI_INDEX_ID          = 0x00;
inline constexpr uint16_t VBE_DISPI_INDEX_XRES        = 0x01;
inline constexpr uint16_t VBE_DISPI_INDEX_YRES        = 0x02;
inline constexpr uint16_t VBE_DISPI_INDEX_BPP         = 0x03;
inline constexpr uint16_t VBE_DISPI_INDEX_ENABLE      = 0x04;
inline constexpr uint16_t VBE_DISPI_INDEX_BANK        = 0x05;
inline constexpr uint16_t VBE_DISPI_INDEX_VIRT_WIDTH  = 0x06;
inline constexpr uint16_t VBE_DISPI_INDEX_VIRT_HEIGHT = 0x07;
inline constexpr uint16_t VBE_DISPI_INDEX_X_OFFSET    = 0x08;
inline constexpr uint16_t VBE_DISPI_INDEX_Y_OFFSET    = 0x09;

inline constexpr uint16_t VBE_DISPI_DISABLED    = 0x00;
inline constexpr uint16_t VBE_DISPI_ENABLED     = 0x01;
inline constexpr uint16_t VBE_DISPI_LFB_ENABLED = 0x40;
inline constexpr uint16_t VBE_DISPI_NOCLEARMEM  = 0x80;

inline constexpr uint16_t VBE_DISPI_BPP_32      = 0x20;

inline constexpr uint64_t VBE_LFB_PHYSICAL_ADDR = 0xE0000000ULL;
inline constexpr uint64_t VBE_LFB_VIRTUAL_ADDR  = 0xFFFFFFFFE0000000ULL;

struct FramebufferInfo {
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
    uint32_t bytes_per_pixel;
    uint32_t* back_buffer;
    uint32_t* front_buffer;
    bool is_active;
};

void             vbe_init(void);
bool             vbe_is_available(void);
bool             vbe_set_mode(uint32_t width, uint32_t height, uint32_t bpp);
void             vbe_set_mode_disabled(void);
uint64_t         vbe_find_lfb_paddr(void);
FramebufferInfo* vbe_get_info(void);
void             vbe_swap_buffers(void);
void             vbe_swap_rect(int x, int y, int w, int h);
void             vbe_clear(uint32_t color);

}
