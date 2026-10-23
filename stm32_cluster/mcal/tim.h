/**
 * @file    tim.h
 * @brief   Timer usage on the cluster:
 *          TIM1_CH1 (PA8)  backlight PWM 20 kHz
 *          TIM4_CH1 (PB6)  buzzer PWM (audible tone, 50 % duty when on)
 *          TIM2_CH2 (PA1)  wheel-speed pulse input capture, 1 MHz time base
 */
#ifndef TIM_H
#define TIM_H

#include "std_types.h"

void     tim_backlight_init(void);
void     tim_backlight_set(uint8_t percent);

void     tim_buzzer_init(uint32_t tone_hz);
void     tim_buzzer_on(bool on);

void     tim_capture_init(void);
/**
 * Latest pulse period in microseconds and the tick (ms) of the last edge.
 * Returns false before two edges have been seen.
 */
bool     tim_capture_get(uint32_t *period_us, uint32_t *last_edge_ms);

#endif /* TIM_H */
