/**
 * @file    spi.c
 * @brief   SPI1 + DMA driver (see spi.h).
 */
#include "spi.h"
#include "gpio.h"
#include "stm32f1xx.h"

#define DMA_MAX_CHUNK   (0xFFFFUL)      /* CNDTR is 16 bit */

static void spi_wait_idle(void)
{
    /* TXE alone is not enough: BSY stays set until the last bit is shifted out */
    while ((SPI1->SR & SPI_SR_TXE) == 0UL) { }
    while ((SPI1->SR & SPI_SR_BSY) != 0UL) { }
}

static void spi_set_16bit(bool on)
{
    SPI1->CR1 &= ~SPI_CR1_SPE;          /* DFF may only change while SPE = 0 */
    if (on) { SPI1->CR1 |= SPI_CR1_DFF; }
    else    { SPI1->CR1 &= ~SPI_CR1_DFF; }
    SPI1->CR1 |= SPI_CR1_SPE;
}

static void spi_clear_ovr(void)
{
    /* TX-only traffic overruns the RX side; OVR is cleared by reading DR then SR */
    (void)SPI1->DR;
    (void)SPI1->SR;
}

void spi_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    RCC->AHBENR  |= RCC_AHBENR_DMA1EN;
    gpio_config(GPIOA, 5U, GPIO_AF_PP_50MHZ);
    gpio_config(GPIOA, 6U, GPIO_IN_FLOATING);
    gpio_config(GPIOA, 7U, GPIO_AF_PP_50MHZ);

    /* Master, CPOL=0 CPHA=0, fPCLK2/2 = 36 MHz, software NSS (SSM+SSI) */
    SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | (0UL << SPI_CR1_BR_Pos);
    SPI1->CR1 |= SPI_CR1_SPE;
}

void spi_write8(const uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0U; i < len; i++)
    {
        while ((SPI1->SR & SPI_SR_TXE) == 0UL) { }
        *(volatile uint8_t *)&SPI1->DR = data[i];   /* 8-bit access keeps DR byte-wide */
    }
    spi_wait_idle();
    spi_clear_ovr();
}

void spi_write16_dma(const uint16_t *src, uint32_t count, bool minc)
{
    spi_set_16bit(true);
    SPI1->CR2 |= SPI_CR2_TXDMAEN;

    while (count > 0UL)
    {
        const uint32_t chunk = (count > DMA_MAX_CHUNK) ? DMA_MAX_CHUNK : count;

        DMA1_Channel3->CCR   = 0UL;
        DMA1->IFCR           = DMA_IFCR_CGIF3;
        DMA1_Channel3->CPAR  = (uint32_t)&SPI1->DR;
        DMA1_Channel3->CMAR  = (uint32_t)src;
        DMA1_Channel3->CNDTR = chunk;
        DMA1_Channel3->CCR   = DMA_CCR_DIR | DMA_CCR_MSIZE_0 | DMA_CCR_PSIZE_0 |
                               DMA_CCR_PL_1 | (minc ? DMA_CCR_MINC : 0UL) | DMA_CCR_EN;

        while ((DMA1->ISR & (DMA_ISR_TCIF3 | DMA_ISR_TEIF3)) == 0UL) { }
        DMA1_Channel3->CCR = 0UL;

        if (minc)
        {
            src = &src[chunk];
        }
        count -= chunk;
    }

    spi_wait_idle();                    /* DMA TC = last word written to DR, not sent */
    SPI1->CR2 &= ~SPI_CR2_TXDMAEN;
    spi_set_16bit(false);
    spi_clear_ovr();
}
