/**
 * @file    gfx.h
 * @brief   Minimal 2D primitives on top of the ILI9341 driver (no framebuffer: 150 KB would
 *          not fit in 20 KB RAM). Text is rendered one scan line at a time into a 640-byte
 *          line buffer and streamed with DMA.
 */
#ifndef GFX_H
#define GFX_H

#include "std_types.h"

void     gfx_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void     gfx_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
/** Text with integer scale (1..8). Glyph cell = 8*scale square, background painted. */
void     gfx_text(uint16_t x, uint16_t y, const char *s, uint8_t scale, uint16_t fg, uint16_t bg);
uint16_t gfx_text_width(const char *s, uint8_t scale);
/**
 * Horizontal bar gauge: draws only the part that changed between old_fill and new_fill
 * (both in pixels, 0..w) to avoid flicker.
 */
void     gfx_bar_update(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                        uint16_t old_fill, uint16_t new_fill, uint16_t fg, uint16_t bg);

#endif /* GFX_H */
