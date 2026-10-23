/**
 * @file    exti.h
 * @brief   External interrupt lines 10..15 (EXTI15_10 vector) with per-line callbacks.
 */
#ifndef EXTI_H
#define EXTI_H

#include "std_types.h"
#include "stm32f1xx.h"

typedef void (*exti_cb_t)(uint8_t line);

typedef enum
{
    EXTI_EDGE_RISING  = 1,
    EXTI_EDGE_FALLING = 2,
    EXTI_EDGE_BOTH    = 3
} exti_edge_t;

/** port_index: 0 = GPIOA, 1 = GPIOB, 2 = GPIOC (AFIO_EXTICRx encoding). line: 10..15 */
std_return_t exti_config(uint8_t port_index, uint8_t line, exti_edge_t edge, exti_cb_t cb);

#endif /* EXTI_H */
