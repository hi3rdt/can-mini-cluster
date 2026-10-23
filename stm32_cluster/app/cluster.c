/**
 * @file    cluster.c
 * @brief   Cluster application logic (see cluster.h).
 */
#include "cluster.h"
#include "com.h"
#include "adc.h"
#include "can_matrix.h"
#include <string.h>

#define TASK_PERIOD_MS      (20UL)
#define FUEL_IIR_SHIFT      (4U)        /* time constant ~ 16 x 20 ms = 320 ms */
/* Odometer: speed [0.01 km/h] x time [ms] adds up to 0.1 km at 0.1 x 100 x 3 600 000 */
#define ODO_ACC_PER_0_1KM   (36000000UL)

static cluster_state_t s_state;
static uint32_t s_fuel_filt_x256;       /* percent x 256 (Q8 fixed point) */
static uint32_t s_odo_acc;

static uint32_t fuel_pct_x256(void)
{
    return ((uint32_t)adc_read_fuel() * 100UL * 256UL) / ADC_RAW_MAX;
}

static void update_fuel(void)
{
    const uint32_t x = fuel_pct_x256();

    /* First-order low-pass y += (x - y) / 16 so a sloshing tank does not jump the gauge.
       Q8 keeps the fraction that a plain integer division would throw away. */
    if (x >= s_fuel_filt_x256)
    {
        s_fuel_filt_x256 += (x - s_fuel_filt_x256) >> FUEL_IIR_SHIFT;
    }
    else
    {
        s_fuel_filt_x256 -= (s_fuel_filt_x256 - x) >> FUEL_IIR_SHIFT;
    }
    s_state.fuel_pct = (uint8_t)((s_fuel_filt_x256 + 128UL) >> 8);    /* round */
}

static void update_odometer(uint32_t speed_x100)
{
    s_odo_acc += speed_x100 * TASK_PERIOD_MS;
    while (s_odo_acc >= ODO_ACC_PER_0_1KM)
    {
        s_odo_acc -= ODO_ACC_PER_0_1KM;
        s_state.odometer_x10 = (s_state.odometer_x10 + 1UL) & ODO_RAW_MAX;
    }
}

static void update_warnings(void)
{
    uint8_t w = 0U;

    if (s_state.engine_valid && (s_state.speed_kmh > OVERSPEED_KMH))
    {
        w |= WARN_OVERSPEED;
    }
    if (s_state.engine_valid && (s_state.coolant_c > COOLANT_WARN_C))
    {
        w |= WARN_COOLANT_HIGH;
    }
    /* Low fuel with hysteresis: on below 10 %, off only above 12 %, so the lamp
       does not flicker when the level sits right at the threshold. */
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
    if (!s_state.engine_valid)
    {
        w |= WARN_ENGINE_LOST;
    }
    if (!s_state.body_valid)
    {
        w |= WARN_BODY_LOST;
    }
    s_state.warnings = w;
}

void cluster_init(void)
{
    (void)memset(&s_state, 0, sizeof(s_state));
    s_fuel_filt_x256 = fuel_pct_x256();     /* start the filter at the real level */
    s_state.fuel_pct = (uint8_t)((s_fuel_filt_x256 + 128UL) >> 8);
    s_odo_acc = 0UL;
}

void cluster_update_20ms(void)
{
    msg_engine_data_t eng;
    msg_body_status_t body;

    const bool eng_ok  = com_get_engine(&eng);
    const bool body_ok = com_get_body(&body);

    /* Fail-safe: a lost message shows as "---" (dashboard) instead of a frozen value */
    s_state.engine_valid = eng_ok;
    if (eng_ok)
    {
        s_state.speed_kmh = (uint16_t)((eng.vehicle_speed_raw + (SPEED_RAW_PER_KMH / 2U)) / SPEED_RAW_PER_KMH);
        s_state.rpm       = (uint16_t)(eng.engine_speed_raw / ENGINE_RPM_RAW_PER_RPM);
        s_state.coolant_c = (int16_t)((int16_t)eng.coolant_temp_raw - COOLANT_OFFSET_C);
        update_odometer(eng.vehicle_speed_raw);
    }

    s_state.body_valid      = body_ok;
    s_state.turn_left       = body_ok && body.turn_left;
    s_state.turn_right      = body_ok && body.turn_right;
    s_state.high_beam       = body_ok && body.high_beam;
    s_state.door_open       = body_ok && body.door_open;
    s_state.belt_unfastened = body_ok && body.seatbelt_unfastened;

    update_fuel();
    update_warnings();
}

void cluster_send_status_100ms(void)
{
    msg_cluster_status_t m;
    m.fuel_level_pct = s_state.fuel_pct;
    m.odometer_raw   = s_state.odometer_x10;
    m.warning_bits   = s_state.warnings;
    m.dtc_count      = 0U;      /* reserved for diagnostics */
    m.alive          = 0U;      /* filled in by E2E */
    m.crc            = 0U;
    (void)com_send_cluster_status(&m);     /* a busy mailbox only skips one 100 ms cycle */
}

const cluster_state_t *cluster_state(void)
{
    return &s_state;
}
