/**
 * @file    gfx.c
 * @brief   2D primitives (see gfx.h).
 */
#include "gfx.h"
#include "font8x8.h"
#include "ili9341.h"

#define GFX_MAX_SCALE   (8U)

static uint16_t s_line[LCD_WIDTH];      /* one scan line, DMA source */

void gfx_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    ili9341_fill_rect(x, y, w, h, color);
}

void gfx_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    if ((w < 2U) || (h < 2U))
    {
        return;
    }
    ili9341_fill_rect(x, y, w, 1U, color);
    ili9341_fill_rect(x, (uint16_t)(y + h - 1U), w, 1U, color);
    ili9341_fill_rect(x, y, 1U, h, color);
    ili9341_fill_rect((uint16_t)(x + w - 1U), y, 1U, h, color);
}

uint16_t gfx_text_width(const char *s, uint8_t scale)
{
    uint16_t n = 0U;
    while (s[n] != '\0')
    {
        n++;
    }
    return (uint16_t)(n * 8U * scale);
}

void gfx_text(uint16_t x, uint16_t y, const char *s, uint8_t scale, uint16_t fg, uint16_t bg)
{
    if ((scale == 0U) || (scale > GFX_MAX_SCALE) || (x >= LCD_WIDTH))
    {
        return;
    }

    /* Clip the string to the line buffer / screen width */
    uint16_t w = gfx_text_width(s, scale);
    if ((uint32_t)x + w > LCD_WIDTH)
    {
        w = (uint16_t)(((LCD_WIDTH - x) / (8U * scale)) * 8U * scale);
    }
    const uint16_t h = (uint16_t)(8U * scale);
    if ((w == 0U) || ((uint32_t)y + h > LCD_HEIGHT))
    {
        return;
    }
    const uint16_t nchars = (uint16_t)(w / (8U * scale));

    ili9341_set_window(x, y, (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U));
    for (uint16_t py = 0U; py < h; py++)
    {
        const uint8_t row = (uint8_t)(py / scale);
        uint16_t px = 0U;

        for (uint16_t c = 0U; c < nchars; c++)
        {
            uint8_t ch = (uint8_t)s[c];
            if ((ch < FONT_FIRST_CHAR) || (ch > FONT_LAST_CHAR))
            {
                ch = (uint8_t)'?';
            }
            const uint8_t bits = g_font8x8[ch - FONT_FIRST_CHAR][row];
            for (uint8_t bx = 0U; bx < 8U; bx++)
            {
                const uint16_t color = (((bits >> bx) & 1U) != 0U) ? fg : bg;
                for (uint8_t k = 0U; k < scale; k++)
                {
                    s_line[px] = color;
                    px++;
                }
            }
        }
        ili9341_push_pixels(s_line, w);
    }
    ili9341_end();
}

void gfx_bar_update(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                    uint16_t old_fill, uint16_t new_fill, uint16_t fg, uint16_t bg)
{
    if (old_fill > w) { old_fill = w; }
    if (new_fill > w) { new_fill = w; }

    if (new_fill > old_fill)
    {
        ili9341_fill_rect((uint16_t)(x + old_fill), y, (uint16_t)(new_fill - old_fill), h, fg);
    }
    else if (new_fill < old_fill)
    {
        ili9341_fill_rect((uint16_t)(x + new_fill), y, (uint16_t)(old_fill - new_fill), h, bg);
    }
    else
    {
        /* unchanged: nothing to draw */
    }
}
