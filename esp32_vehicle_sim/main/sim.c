/**
 * @file    sim.c
 * @brief   Vehicle model and periodic senders (see sim.h).
 */
#include "sim.h"
#include "app_config.h"
#include "can_if.h"
#include "can_matrix.h"
#include "e2e.h"

#include <math.h>
#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "sim";

#define SWEEP_PERIOD_S      (40.0f)     /* 0 -> 140 -> 0 km/h */
#define SWEEP_TOP_KMH       (140.0f)
#define IDLE_RPM            (800.0f)
#define RPM_PER_KMH         (38.0f)
#define LEDC_RES            LEDC_TIMER_10_BIT
#define LEDC_DUTY_50        (512U)

static sim_cfg_t s_cfg = {
    .mode = SIM_MODE_SWEEP, .speed_kmh = 0.0f, .rpm = IDLE_RPM, .coolant_c = 20.0f,
    .pulse_enabled = true,
};
static uint8_t  s_engine_alive;
static uint8_t  s_body_alive;
static uint32_t s_last_pulse_hz;

sim_cfg_t *sim_cfg(void)
{
    return &s_cfg;
}

static void sweep_step(float t_s)
{
    /* Triangle speed profile, rpm follows speed with a crude "gear" sawtooth */
    const float phase = fmodf(t_s, SWEEP_PERIOD_S) / SWEEP_PERIOD_S;
    const float tri   = (phase < 0.5f) ? (phase * 2.0f) : (2.0f - (phase * 2.0f));
    s_cfg.speed_kmh = tri * SWEEP_TOP_KMH;

    const float gear_band = fmodf(s_cfg.speed_kmh, 35.0f) / 35.0f;
    s_cfg.rpm = IDLE_RPM + (gear_band * 4500.0f) + (s_cfg.speed_kmh * 10.0f);

    /* warm-up 20 -> 92 degC in ~90 s */
    if (s_cfg.coolant_c < 92.0f)
    {
        s_cfg.coolant_c += 0.016f;
    }

    const uint32_t slot = (uint32_t)(t_s / 8.0f) % 6U;
    s_cfg.turn_left       = (slot == 1U);
    s_cfg.turn_right      = (slot == 3U);
    s_cfg.high_beam       = (slot == 4U);
    s_cfg.belt_unfastened = (slot == 5U);
    s_cfg.door_open       = (slot == 0U) && (s_cfg.speed_kmh < 5.0f);
}

static void update_pulse(void)
{
    const float v = s_cfg.speed_kmh + s_cfg.pulse_offset_kmh;
    const uint32_t hz = (s_cfg.pulse_enabled && (v >= 1.0f))
                      ? (uint32_t)lroundf(v * (float)SPEED_PULSE_HZ_PER_KMH) : 0U;

    if (hz == s_last_pulse_hz)
    {
        return;
    }
    if (hz == 0U)
    {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    }
    else
    {
        ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, hz);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, LEDC_DUTY_50);
    }
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    s_last_pulse_hz = hz;
}

static void send_engine(void)
{
    msg_engine_data_t m = { 0 };
    uint8_t buf[DLC_ENGINE_DATA];

    const float rpm = fmaxf(0.0f, fminf(s_cfg.rpm, 16383.0f));
    const float spd = fmaxf(0.0f, fminf(s_cfg.speed_kmh, 655.0f));
    const float tmp = fmaxf(-40.0f, fminf(s_cfg.coolant_c, 215.0f));

    m.engine_speed_raw  = (uint16_t)lroundf(rpm * (float)ENGINE_RPM_RAW_PER_RPM);
    m.vehicle_speed_raw = (uint16_t)lroundf(spd * (float)SPEED_RAW_PER_KMH);
    m.coolant_temp_raw  = (uint8_t)lroundf(tmp + (float)COOLANT_OFFSET_C);
    can_pack_engine_data(&m, buf);
    e2e_protect(CANID_ENGINE_DATA, buf, DLC_ENGINE_DATA, ENGINE_DATA_CRC_POS,
                ENGINE_DATA_ALIVE_POS, &s_engine_alive);
    if (s_cfg.corrupt_crc)
    {
        buf[ENGINE_DATA_CRC_POS] ^= 0x5AU;
    }
    (void)can_if_send(CANID_ENGINE_DATA, buf, DLC_ENGINE_DATA);
}

static void send_body(void)
{
    msg_body_status_t m = {
        .turn_left = s_cfg.turn_left, .turn_right = s_cfg.turn_right,
        .high_beam = s_cfg.high_beam, .door_open = s_cfg.door_open,
        .seatbelt_unfastened = s_cfg.belt_unfastened,
    };
    uint8_t buf[DLC_BODY_STATUS];
    can_pack_body_status(&m, buf);
    e2e_protect(CANID_BODY_STATUS, buf, DLC_BODY_STATUS, BODY_STATUS_CRC_POS,
                BODY_STATUS_ALIVE_POS, &s_body_alive);
    (void)can_if_send(CANID_BODY_STATUS, buf, DLC_BODY_STATUS);
}

static void sim_task(void *arg)
{
    (void)arg;
    TickType_t wake = xTaskGetTickCount();
    uint32_t cycle = 0U;

    for (;;)
    {
        /* 20 ms base cycle, absolute timing: no drift from the work done in the loop */
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(CYCLE_ENGINE_DATA_MS));
        cycle++;

        if (s_cfg.mode == SIM_MODE_SWEEP)
        {
            sweep_step((float)cycle * ((float)CYCLE_ENGINE_DATA_MS / 1000.0f));
        }
        if (!s_cfg.mute_engine)
        {
            send_engine();
        }
        if (((cycle % (CYCLE_BODY_STATUS_MS / CYCLE_ENGINE_DATA_MS)) == 0U) && !s_cfg.mute_body)
        {
            send_body();
        }
        update_pulse();
    }
}

void sim_init(void)
{
    const ledc_timer_config_t t = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_RES,
        .timer_num       = LEDC_TIMER_0,
        .freq_hz         = 100,
        .clk_cfg         = LEDC_USE_REF_TICK,   /* 1 MHz: allows ~1..976 Hz at 10 bit */
    };
    ESP_ERROR_CHECK(ledc_timer_config(&t));
    const ledc_channel_config_t c = {
        .gpio_num   = PIN_SPEED_PULSE,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_0,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = 0,
        .hpoint     = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&c));

    xTaskCreate(sim_task, "sim", 4096, NULL, TASK_PRIO_SIM, NULL);
    ESP_LOGI(TAG, "vehicle simulator running (mode: sweep)");
}
