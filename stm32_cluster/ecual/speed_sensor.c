/**
 * @file    speed_sensor.c
 * @brief   Wheel-speed conversion (see speed_sensor.h).
 *
 * f [Hz] = 1e6 / period_us, v [km/h] = f / SPEED_PULSE_HZ_PER_KMH
 * => v_x100 = 1e8 / (period_us * SPEED_PULSE_HZ_PER_KMH)
 */
#include "speed_sensor.h"
#include "tim.h"
#include "board.h"

void speed_sensor_init(void)
{
    tim_capture_init();
}

bool speed_sensor_get_x100(uint32_t now_ms, uint32_t *speed_x100)
{
    uint32_t period_us;
    uint32_t last_ms;

    if (!tim_capture_get(&period_us, &last_ms))
    {
        *speed_x100 = 0UL;
        return false;
    }
    if (((uint32_t)(now_ms - last_ms) >= SPEED_SENSOR_STANDSTILL_MS) || (period_us == 0UL))
    {
        *speed_x100 = 0UL;   /* stopped: pulses ceased */
    }
    else
    {
        *speed_x100 = 100000000UL / (period_us * SPEED_PULSE_HZ_PER_KMH);
    }
    return true;
}
