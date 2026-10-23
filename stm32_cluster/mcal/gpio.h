/**
 * @file    gpio.h
 * @brief   GPIO configuration for STM32F1 (CRL/CRH 4-bit CNF:MODE fields) and fast pin access.
 */
#ifndef GPIO_H
#define GPIO_H

#include "std_types.h"
#include "stm32f1xx.h"

/* CNF[1:0]:MODE[1:0] nibbles, RM0008 9.2.1 */
#define GPIO_IN_ANALOG      (0x0U)
#define GPIO_IN_FLOATING    (0x4U)
#define GPIO_IN_PULL        (0x8U)  /* pull-up/down selected by ODR */
#define GPIO_OUT_PP_2MHZ    (0x2U)
#define GPIO_OUT_PP_50MHZ   (0x3U)
#define GPIO_AF_PP_50MHZ    (0xBU)
#define GPIO_AF_OD_50MHZ    (0xFU)

void gpio_clock_enable_all(void);
void gpio_config(GPIO_TypeDef *port, uint8_t pin, uint8_t mode);
void gpio_config_pullup(GPIO_TypeDef *port, uint8_t pin);

/* BSRR/BRR writes are atomic: safe from ISR and main without critical sections */
static inline void gpio_set(GPIO_TypeDef *port, uint8_t pin)   { port->BSRR = (1UL << pin); }
static inline void gpio_clear(GPIO_TypeDef *port, uint8_t pin) { port->BRR  = (1UL << pin); }
static inline bool gpio_read(const GPIO_TypeDef *port, uint8_t pin)
{
    return (port->IDR & (1UL << pin)) != 0UL;
}
static inline void gpio_toggle(GPIO_TypeDef *port, uint8_t pin)
{
    /* Read-modify of ODR, but expressed as one atomic BSRR write */
    const uint32_t odr = port->ODR;
    port->BSRR = ((odr & (1UL << pin)) << 16) | (~odr & (1UL << pin));
}

#endif /* GPIO_H */
