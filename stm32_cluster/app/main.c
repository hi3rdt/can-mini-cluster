/**
 * @file    main.c
 * @brief   CAN mini instrument cluster ECU - STM32F103C8T6, bare-metal (no HAL/LL).
 *
 * Super loop with fixed time slots, all driven by the 1 ms SysTick counter:
 *   every pass : drain the CAN RX queue (frames are queued by the RX0 ISR)
 *   10 ms      : message timeout monitoring
 *   20 ms      : cluster logic + dashboard partial redraw (50 Hz)
 *   100 ms     : ClusterStatus TX, heartbeat LED
 *   1000 ms    : status line on the debug UART
 * The watchdog is refreshed once per pass, so a hang anywhere resets the MCU.
 */
#include "board.h"
#include "rcc.h"
#include "gpio.h"
#include "systick.h"
#include "uart.h"
#include "adc.h"
#include "spi.h"
#include "bxcan.h"
#include "iwdg.h"
#include "ili9341.h"
#include "com.h"
#include "log.h"
#include "cluster.h"
#include "dashboard.h"

#define NVIC_GROUP_4_PREEMPT_BITS   (3U)    /* PRIGROUP=3: 4 bits pre-emption, 0 sub */

typedef struct
{
    uint32_t period_ms;
    uint32_t next_ms;   /* release time of the next run */
} slot_t;

/* True when the slot is due; the next release is planned from the previous one,
   not from 'now', so the period does not drift when a pass runs late. */
static bool slot_due(slot_t *s, uint32_t now)
{
    if ((int32_t)(now - s->next_ms) < 0)
    {
        return false;
    }
    s->next_ms += s->period_ms;
    return true;
}

static void fatal_clock_failure(void)
{
    /* No crystal: CAN bit timing would be wrong, so do not join the bus at all.
       Fast LED blink until power cycle. */
    gpio_clock_enable_all();
    gpio_config(LED_PORT, LED_PIN, GPIO_OUT_PP_2MHZ);
    for (;;)
    {
        gpio_toggle(LED_PORT, LED_PIN);
        for (volatile uint32_t i = 0UL; i < 100000UL; i++) { }
    }
}

static void log_status(void)
{
    com_stats_t cs;
    can_stats_t bs;
    const cluster_state_t *st = cluster_state();

    com_get_stats(&cs);
    can_get_stats(&bs);
    log_printf("t=%lus spd=%u rpm=%u fuel=%u%% eng=%s body=%s | rx %lu/%lu crc_err %lu seq_err %lu "
               "timeouts %lu/%lu | TEC %u REC %u busoff %lu fifo_ovr %lu q_ovf %lu\r\n",
               (unsigned long)(tick_get() / 1000UL), (unsigned)st->speed_kmh, (unsigned)st->rpm,
               (unsigned)st->fuel_pct, st->engine_valid ? "ok" : "LOST", st->body_valid ? "ok" : "LOST",
               (unsigned long)cs.engine_rx, (unsigned long)cs.body_rx,
               (unsigned long)cs.e2e_crc_errors, (unsigned long)cs.e2e_seq_errors,
               (unsigned long)cs.engine_timeouts, (unsigned long)cs.body_timeouts,
               (unsigned)bs.tec, (unsigned)bs.rec, (unsigned long)bs.bus_off_events,
               (unsigned long)bs.rx_fifo_overruns, (unsigned long)bs.rx_queue_overflows);
}

int main(void)
{
    if (rcc_init_72mhz() != E_OK)
    {
        fatal_clock_failure();
    }
    NVIC_SetPriorityGrouping(NVIC_GROUP_4_PREEMPT_BITS);
    gpio_clock_enable_all();
    gpio_config(LED_PORT, LED_PIN, GPIO_OUT_PP_2MHZ);
    gpio_set(LED_PORT, LED_PIN);                    /* LED off (active low) */
    systick_init(HCLK_HZ);

    uart_init(PCLK2_HZ, UART_BAUD);
    const reset_cause_t rc = rcc_reset_cause();
    log_printf("\r\nCAN mini cluster %s, reset cause: %s\r\n", FW_VERSION_STR, rcc_reset_cause_str(rc));

    if (adc_init() != E_OK)
    {
        log_printf("ADC calibration timeout\r\n");
    }

    gpio_config(TFT_BL_PORT, TFT_BL_PIN, GPIO_OUT_PP_2MHZ);
    gpio_set(TFT_BL_PORT, TFT_BL_PIN);              /* backlight on */
    spi_init();
    ili9341_init();
    dashboard_init();

    if (can_init(CAN_MODE_NORMAL) != E_OK)
    {
        log_printf("CAN init timeout: check transceiver and wiring (RX must idle high)\r\n");
    }
    com_init(tick_get());
    cluster_init();

    /* Started last: the display init above takes longer than the watchdog timeout */
    iwdg_start(WATCHDOG_TIMEOUT_MS);

    const uint32_t t0 = tick_get();
    slot_t s10   = { 10UL,   t0 + 1UL };            /* offsets: slots never all fall due */
    slot_t s20   = { 20UL,   t0 + 3UL };            /* in the same pass                  */
    slot_t s100  = { 100UL,  t0 + 7UL };
    slot_t s1000 = { 1000UL, t0 + 500UL };
    uint8_t hb = 0U;

    for (;;)
    {
        const uint32_t now = tick_get();

        com_poll_rx(now);
        if (slot_due(&s10, now))
        {
            com_check_timeouts(now);
        }
        if (slot_due(&s20, now))
        {
            cluster_update_20ms();
            dashboard_update(cluster_state(), now);
        }
        if (slot_due(&s100, now))
        {
            cluster_send_status_100ms();
            hb++;
            if ((hb % 5U) == 0U)
            {
                gpio_toggle(LED_PORT, LED_PIN);     /* 1 Hz heartbeat */
            }
        }
        if (slot_due(&s1000, now))
        {
            log_status();
        }
        iwdg_refresh();
    }
}
