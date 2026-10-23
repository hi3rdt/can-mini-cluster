/**
 * @file    ili9341.h
 * @brief   ILI9341 320x240 TFT, landscape, RGB565 over SPI1 (+DMA for pixel data).
 */
#ifndef ILI9341_H
#define ILI9341_H

#include "std_types.h"

#define LCD_WIDTH   (320U)
#define LCD_HEIGHT  (240U)

/* RGB565 helpers */
#define RGB565(r, g, b) ((uint16_t)((((uint16_t)(r) & 0xF8U) << 8) | \
                                    (((uint16_t)(g) & 0xFCU) << 3) | ((uint16_t)(b) >> 3)))
#define COLOR_BLACK     (0x0000U)
#define COLOR_WHITE     (0xFFFFU)
#define COLOR_RED       (0xF800U)
#define COLOR_GREEN     (0x07E0U)
#define COLOR_BLUE      (0x001FU)
#define COLOR_AMBER     RGB565(255, 176, 0)
#define COLOR_GREY      RGB565(90, 90, 90)
#define COLOR_DARK      RGB565(24, 24, 32)
#define COLOR_CYAN      RGB565(0, 200, 255)

void ili9341_init(void);
/** Select the drawing window (inclusive coordinates) and start RAM write. */
void ili9341_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void ili9341_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
/** Stream pixels into the window opened by ili9341_set_window (CS kept low). */
void ili9341_push_pixels(const uint16_t *px, uint32_t count);
void ili9341_end(void);

#endif /* ILI9341_H */
