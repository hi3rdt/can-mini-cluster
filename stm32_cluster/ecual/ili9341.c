/**
 * @file    ili9341.c
 * @brief   ILI9341 driver (see ili9341.h). Command set: ILI9341 datasheet V1.11 section 8.
 */
#include "ili9341.h"
#include "spi.h"
#include "gpio.h"
#include "systick.h"
#include "board.h"

#define CMD_SWRESET     (0x01U)
#define CMD_SLPOUT      (0x11U)
#define CMD_DISPON      (0x29U)
#define CMD_CASET       (0x2AU)
#define CMD_PASET       (0x2BU)
#define CMD_RAMWR       (0x2CU)
#define CMD_MADCTL      (0x36U)
#define CMD_COLMOD      (0x3AU)
#define CMD_FRMCTR1     (0xB1U)
#define CMD_PWCTR1      (0xC0U)
#define CMD_PWCTR2      (0xC1U)
#define CMD_VMCTR1      (0xC5U)
#define CMD_VMCTR2      (0xC7U)

#define MADCTL_MV       (0x20U)     /* row/column exchange -> landscape */
#define MADCTL_MX       (0x40U)
#define MADCTL_BGR      (0x08U)

static inline void cs_low(void)   { gpio_clear(TFT_CS_PORT, TFT_CS_PIN); }
static inline void cs_high(void)  { gpio_set(TFT_CS_PORT, TFT_CS_PIN); }
static inline void dc_cmd(void)   { gpio_clear(TFT_DC_PORT, TFT_DC_PIN); }
static inline void dc_data(void)  { gpio_set(TFT_DC_PORT, TFT_DC_PIN); }

static void write_cmd(uint8_t cmd, const uint8_t *args, uint16_t n)
{
    cs_low();
    dc_cmd();
    spi_write8(&cmd, 1U);
    if (n > 0U)
    {
        dc_data();
        spi_write8(args, n);
    }
    cs_high();
}

void ili9341_init(void)
{
    gpio_config(TFT_CS_PORT, TFT_CS_PIN, GPIO_OUT_PP_50MHZ);
    gpio_config(TFT_DC_PORT, TFT_DC_PIN, GPIO_OUT_PP_50MHZ);
    gpio_config(TFT_RST_PORT, TFT_RST_PIN, GPIO_OUT_PP_2MHZ);
    cs_high();

    /* Hardware reset: low >= 10 us, then 120 ms before SLPOUT is allowed */
    gpio_clear(TFT_RST_PORT, TFT_RST_PIN);
    tick_delay_ms(5U);
    gpio_set(TFT_RST_PORT, TFT_RST_PIN);
    tick_delay_ms(120U);

    write_cmd(CMD_SWRESET, NULL, 0U);
    tick_delay_ms(120U);

    static const uint8_t pwctr1[]  = { 0x23U };             /* GVDD 4.60 V */
    static const uint8_t pwctr2[]  = { 0x10U };
    static const uint8_t vmctr1[]  = { 0x3EU, 0x28U };      /* VCOMH 4.25 V, VCOML -1.5 V */
    static const uint8_t vmctr2[]  = { 0x86U };
    static const uint8_t madctl[]  = { MADCTL_MV | MADCTL_BGR };
    static const uint8_t colmod[]  = { 0x55U };             /* 16 bit/pixel */
    static const uint8_t frmctr1[] = { 0x00U, 0x18U };      /* 79 Hz frame rate */

    write_cmd(CMD_PWCTR1, pwctr1, sizeof(pwctr1));
    write_cmd(CMD_PWCTR2, pwctr2, sizeof(pwctr2));
    write_cmd(CMD_VMCTR1, vmctr1, sizeof(vmctr1));
    write_cmd(CMD_VMCTR2, vmctr2, sizeof(vmctr2));
    write_cmd(CMD_MADCTL, madctl, sizeof(madctl));
    write_cmd(CMD_COLMOD, colmod, sizeof(colmod));
    write_cmd(CMD_FRMCTR1, frmctr1, sizeof(frmctr1));

    write_cmd(CMD_SLPOUT, NULL, 0U);
    tick_delay_ms(120U);
    write_cmd(CMD_DISPON, NULL, 0U);

    ili9341_fill_rect(0U, 0U, LCD_WIDTH, LCD_HEIGHT, COLOR_BLACK);
}

void ili9341_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    const uint8_t col[4] = { (uint8_t)(x0 >> 8), (uint8_t)x0, (uint8_t)(x1 >> 8), (uint8_t)x1 };
    const uint8_t row[4] = { (uint8_t)(y0 >> 8), (uint8_t)y0, (uint8_t)(y1 >> 8), (uint8_t)y1 };
    const uint8_t ramwr  = CMD_RAMWR;

    write_cmd(CMD_CASET, col, 4U);
    write_cmd(CMD_PASET, row, 4U);
    cs_low();
    dc_cmd();
    spi_write8(&ramwr, 1U);
    dc_data();                      /* CS stays low: pixel data follows */
}

void ili9341_push_pixels(const uint16_t *px, uint32_t count)
{
    spi_write16_dma(px, count, true);
}

void ili9341_end(void)
{
    cs_high();
}

void ili9341_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    if ((w == 0U) || (h == 0U) || (x >= LCD_WIDTH) || (y >= LCD_HEIGHT))
    {
        return;
    }
    if ((uint32_t)x + w > LCD_WIDTH)  { w = (uint16_t)(LCD_WIDTH - x); }
    if ((uint32_t)y + h > LCD_HEIGHT) { h = (uint16_t)(LCD_HEIGHT - y); }

    static uint16_t s_color;        /* DMA source must outlive the transfer */
    s_color = color;
    ili9341_set_window(x, y, (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U));
    spi_write16_dma(&s_color, (uint32_t)w * h, false);
    ili9341_end();
}
