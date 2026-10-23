/**
 * @file    cluster.c
 * @brief   Cluster application logic (see cluster.h).
 */
#include "cluster.h"
#include "com.h"
#include "dem.h"
#include "adc.h"
#include "tim.h"
#include "speed_sensor.h"
#include "can_matrix.h"
#include <string.h>

#define TASK_PERIOD_MS      (20UL)
#define FUEL_IIR_SHIFT      (4U)        /* tau ~ 16 x 20 ms = 320 ms */
#define FUEL_RAW_MAX        (4095UL)
/* 0.1 km = speed_x100 [0.01 km/h] * t [ms] summed up to 0.1 * 100 * 3.6e6 */
#define ODO_ACC_PER_0_1KM   (36000000UL)
#define BUZZER_PERIOD_MS    (500UL)

static cluster_state_t s_state;
static uint32_t s_fuel_filt_x256;       /* percent * 256 */
static uint32_t s_odo_acc;
static uint32_t s_implausible_since;
static bool     s_implausible_timing;
static uint32_t s_plausible_since;

static void update_fuel(void)
{
    const uint32_t pct_x256 = ((uint32_t)adc_raw(ADC_CH_FUEL) * 100UL * 256UL) / FUEL_RAW_MAX;

    /* first-order IIR: y += (x - y) / 16, in Q8 to keep resolution */
    if (pct_x256 >= s_fuel_filt_x256)
    {
        s_fuel_filt_x256 += (pct_x256 - s_fuel_filt_x256) >> FUEL_IIR_SHIFT;
    }
    else
    {
        s_fuel_filt_x256 -= (s_fuel_filt_x256 - pct_x256) >> FUEL_IIR_SHIFT;
    }
    s_state.fuel_pct = (uint8_t)((s_fuel_filt_x256 + 128UL) >> 8);
}

static void check_plausibility(bool can_ok, uint32_t can_x100, bool wheel_ok,
                               uint32_t wheel_x100, uint32_t now)
{
    if (!can_ok || !wheel_ok)
    {
        s_implausible_timing = false;
        return;     /* no comparison possible: no verdict */
    }
    const uint32_t diff = (can_x100 > wheel_x100) ? (can_x100 - wheel_x100) : (wheel_x100 - can_x100);

    if (diff > (PLAUSIBILITY_DIFF_KMH * SPEED_RAW_PER_KMH))
    {
        if (!s_implausible_timing)
        {
            s_implausible_timing = true;
            s_implausible_since  = now;
        }
        else if ((uint32_t)(now - s_implausible_since) >= PLAUSIBILITY_TIME_MS)
        {
            dem_report(DEM_EVT_SPEED_PLAUSIBILITY, true);
        }
        else
        {
            /* debouncing */
        }
        s_plausible_since = now;
    }
    else
    {
        s_implausible_timing = false;
        if ((uint32_t)(now - s_plausible_since) >= PLAUSIBILITY_TIME_MS)
        {
            dem_report(DEM_EVT_SPEED_PLAUSIBILITY, false);
        }
    }
}

static void update_odometer(uint32_t speed_x100)
{
    s_odo_acc += speed_x100 * TASK_PERIOD_MS;
    while (s_odo_acc >= ODO_ACC_PER_0_1KM)
    {
        s_odo_acc -= ODO_ACC_PER_0_1KM;
        s_state.odometer_x10 = (s_state.odometer_x10 + 1UL) & ODO_RAW_MAX;
        s_state.trip_x10++;
    }
}

static void update_warnings(uint32_t now)
{
    uint8_t w = 0U;

    if ((s_state.speed_src != SPEED_SRC_NONE) && (s_state.speed_kmh > OVERSPEED_KMH))
    {
        w |= WARN_OVERSPEED;
    }
    if (s_state.coolant_valid && (s_state.coolant_c > COOLANT_WARN_C))
    {
        w |= WARN_COOLANT_HIGH;
    }
    /* Low fuel with hysteresis: a sloshing tank must not toggle the lamp */
    if (s_state.fuel_pct < LOW_FUEL_ON_PCT)
    {
        w |= WARN_LOW_FUEL;
    }
    else if (((s_state.warnings & WARN_LOW_FUEL) != 0U) && (s_state.fuel_pct < LOW_FUEL_OFF_PCT))
    {
        w |= WARN_LOW_FUEL;
    }
    else
    {
        /* off */
    }
    if (!s_state.rpm_valid)
    {
        w |= WARN_ENGINE_LOST;
    }
    if (!s_state.body_valid)
    {
        w |= WARN_BODY_LOST;
    }
    if (s_state.speed_src == SPEED_SRC_WHEEL)
    {
        w |= WARN_SPEED_DEGRADED;
    }
    s_state.warnings = w;

    /* Acoustic: overspeed or unbelted above 20 km/h, 2 Hz beep */
    const bool belt_chime = s_state.body_valid && s_state.belt_unfastened &&
                            (s_state.speed_src != SPEED_SRC_NONE) && (s_state.speed_kmh > BELT_CHIME_KMH);
    const bool beep = (((w & WARN_OVERSPEED) != 0U) || belt_chime) &&
                      ((now % BUZZER_PERIOD_MS) < (BUZZER_PERIOD_MS / 2UL));
    tim_buzzer_on(beep);
}

void cluster_init(void)
{
    (void)memset(&s_state, 0, sizeof(s_state));
    s_fuel_filt_x256 = ((uint32_t)adc_raw(ADC_CH_FUEL) * 100UL * 256UL) / FUEL_RAW_MAX;
    s_odo_acc = 0UL;
    s_implausible_timing = false;
    s_plausible_since = 0UL;
}

void cluster_main_20ms(uint32_t now_ms)
{
    msg_engine_data_t eng;
    msg_body_status_t body;
    uint32_t wheel_x100 = 0UL;

    const bool eng_ok   = com_get_engine(&eng);
    const bool body_ok  = com_get_body(&body);
    const bool wheel_ok = speed_sensor_get_x100(now_ms, &wheel_x100);

    /* Speed source selection with graceful degradation */
    uint32_t speed_x100 = 0UL;
    if (eng_ok)
    {
        s_state.speed_src = SPEED_SRC_CAN;
        speed_x100 = eng.vehicle_speed_raw;
    }
    else if (wheel_ok)
    {
        s_state.speed_src = SPEED_SRC_WHEEL;
        speed_x100 = wheel_x100;
    }
    else
    {
        s_state.speed_src = SPEED_SRC_NONE;
    }
    s_state.speed_kmh = (uint16_t)((speed_x100 + (SPEED_RAW_PER_KMH / 2U)) / SPEED_RAW_PER_KMH);
    check_plausibility(eng_ok, eng.vehicle_speed_raw, wheel_ok, wheel_x100, now_ms);

    s_state.rpm_valid     = eng_ok;
    s_state.rpm           = (uint16_t)(eng.engine_speed_raw / ENGINE_RPM_RAW_PER_RPM);
    s_state.coolant_valid = eng_ok;
    s_state.coolant_c     = (int16_t)((int16_t)eng.coolant_temp_raw - COOLANT_OFFSET_C);

    s_state.body_valid      = body_ok;
    s_state.turn_left       = body_ok && body.turn_left;
    s_state.turn_right      = body_ok && body.turn_right;
    s_state.high_beam       = body_ok && body.high_beam;
    s_state.door_open       = body_ok && body.door_open;
    s_state.belt_unfastened = body_ok && body.seatbelt_unfastened;

    update_fuel();
    if (s_state.speed_src != SPEED_SRC_NONE)
    {
        update_odometer(speed_x100);
    }
    s_state.dtc_count = dem_confirmed_count();
    update_warnings(now_ms);
}

void cluster_main_100ms(uint32_t now_ms)
{
    (void)now_ms;
    msg_cluster_status_t m;
    m.fuel_level_pct = s_state.fuel_pct;
    m.odometer_raw   = s_state.odometer_x10;
    m.warning_bits   = s_state.warnings;
    m.dtc_count      = s_state.dtc_count;
    m.alive          = 0U;      /* filled by E2E */
    m.crc            = 0U;
    (void)com_send_cluster_status(&m);
}

const cluster_state_t *cluster_state(void)
{
    return &s_state;
}

void cluster_reset_trip(void)
{
    s_state.trip_x10 = 0UL;
}
