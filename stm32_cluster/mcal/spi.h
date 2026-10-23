/**
 * @file    spi.h
 * @brief   SPI1 master (PA5 SCK, PA6 MISO, PA7 MOSI), mode 0, 36 MHz, TX via DMA1_Channel3.
 */
#ifndef SPI_H
#define SPI_H

#include "std_types.h"

void spi_init(void);
void spi_write8(const uint8_t *data, uint16_t len);         /* polled, for short commands */
/**
 * Send 'count' 16-bit words through DMA. minc = false repeats *src (solid fill).
 * Blocks until the last bit is on the wire (DMA does the copying, the CPU only waits).
 */
void spi_write16_dma(const uint16_t *src, uint32_t count, bool minc);

#endif /* SPI_H */
