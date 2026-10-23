/**
 * @file    systick.h
 * @brief   1 ms system tick and wrap-safe time helpers.
 */
#ifndef SYSTICK_H
#define SYSTICK_H

#include "std_types.h"

void     systick_init(uint32_t hclk_hz);
uint32_t tick_get(void);
bool     tick_elapsed(uint32_t start_ms, uint32_t period_ms);
void     tick_delay_ms(uint32_t ms);     /* init-time use only */

#endif /* SYSTICK_H */
