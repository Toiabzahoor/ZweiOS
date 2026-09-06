#pragma once

#include <stdint.h>
#include <stddef.h>

namespace gui {

inline constexpr int FONT_WIDTH  = 8;
inline constexpr int FONT_HEIGHT = 16;

const uint8_t* font_get_glyph(unsigned char c);

}
