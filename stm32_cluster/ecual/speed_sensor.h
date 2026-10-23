/**
 * @file    speed_sensor.h
 * @brief   Wheel-speed from pulse period (TIM2 input capture), SPEED_PULSE_HZ_PER_KMH scaling.
 */
#ifndef SPEED_SENSOR_H
#define SPEED_SENSOR_H

#include "std_types.h"

#define SPEED_SENSOR_STANDSTILL_MS  (1000UL)    /* no edge for 1 s -> 0 km/h */

void speed_sensor_init(void);
/** Speed in 0.01 km/h. Returns false if no pulse source has ever been seen. */
bool speed_sensor_get_x100(uint32_t now_ms, uint32_t *speed_x100);

#endif /* SPEED_SENSOR_H */
