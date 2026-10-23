#ifndef FONT8X8_H
#define FONT8X8_H

#include <stdint.h>

#define FONT_FIRST_CHAR     (0x20U)
#define FONT_LAST_CHAR      (0x7EU)
#define FONT_NUM_GLYPHS     (FONT_LAST_CHAR - FONT_FIRST_CHAR + 1U)

extern const uint8_t g_font8x8[FONT_NUM_GLYPHS][8];

#endif
