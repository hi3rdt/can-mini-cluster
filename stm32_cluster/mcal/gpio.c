/**
 * @file    gpio.c
 * @brief   GPIO configuration (see gpio.h).
 */
#include "gpio.h"

void gpio_clock_enable_all(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN |
                    RCC_APB2ENR_AFIOEN;
    (void)RCC->APB2ENR;   /* read back: clock is active before the first register access */
}

void gpio_config(GPIO_TypeDef *port, uint8_t pin, uint8_t mode)
{
    volatile uint32_t *cr = (pin < 8U) ? &port->CRL : &port->CRH;
    const uint32_t shift = ((uint32_t)pin & 7UL) * 4UL;

    /* Not atomic: configure pins at init time only, before interrupts are enabled */
    *cr = (*cr & ~(0xFUL << shift)) | ((uint32_t)mode << shift);
}

void gpio_config_pullup(GPIO_TypeDef *port, uint8_t pin)
{
    gpio_config(port, pin, GPIO_IN_PULL);
    gpio_set(port, pin);   /* ODR = 1 selects pull-up */
}
