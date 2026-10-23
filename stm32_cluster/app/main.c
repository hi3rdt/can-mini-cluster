/**
 * @file    main.c
 * @brief   CAN mini instrument cluster ECU - STM32F103C8T6, bare-metal (no HAL/LL).
 *
 * Task set (cooperative, time-triggered):
 *   1 ms   : COM RX dispatch, ISO-TP/UDS timing, CLI
 *   10 ms  : deadline monitoring + DEM, buttons
 *   20 ms  : cluster logic + dashboard partial redraw (50 Hz)
 *   100 ms : ClusterStatus TX, DTC persistence, heartbeat LED, watchdog supervision
 */
#include "board.h"
#include "rcc.h"
#include "gpio.h"
#include "systick.h"
#include "uart.h"
#include "adc.h"
#include "tim.h"
#include "spi.h"
#include "bxcan.h"
#include "fault.h"
#include "ili9341.h"
#include "buttons.h"
#include "speed_sensor.h"
#include "nvm.h"
#include "dem.h"
#include "com.h"
#include "diag.h"
#include "sched.h"
#include "wdg_m.h"
#include "log.h"
#include "cluster.h"
#include "dashboard.h"
#include "cli.h"
#include "app_info.h"

#define ALIVE_1MS       (1UL << 0)
#define ALIVE_10MS      (1UL << 1)
#define ALIVE_20MS      (1UL << 2)
#define ALIVE_100MS     (1UL << 3)
#define ALIVE_ALL       (ALIVE_1MS | ALIVE_10MS | ALIVE_20MS | ALIVE_100MS)

#define NVIC_GROUP_4_PREEMPT_BITS   (3U)    /* PRIGROUP=3: 4 bits pre-emption, 0 sub */

static void task_1ms(uint32_t now)
{
    com_main_1ms(now);
    diag_main_1ms(now);
    cli_poll(now);
    wdg_m_checkpoint(ALIVE_1MS);
}

static void task_10ms(uint32_t now)
{
    com_main_10ms(now);
    if (buttons_take_press(BTN_MENU))
    {
        dashboard_next_info_page();
    }
    if (buttons_take_press(BTN_TRIP))
    {
        cluster_reset_trip();
    }
    wdg_m_checkpoint(ALIVE_10MS);
}

static void task_20ms(uint32_t now)
{
    cluster_main_20ms(now);
    dashboard_update(cluster_state(), now);
    wdg_m_checkpoint(ALIVE_20MS);
}

static void task_100ms(uint32_t now)
{
    static uint8_t s_hb;

    cluster_main_100ms(now);
    dem_main_100ms();
    s_hb++;
    if ((s_hb % 5U) == 0U)
    {
        gpio_toggle(LED_PORT, LED_PIN);     /* 1 Hz heartbeat: scheduler alive */
    }
    wdg_m_checkpoint(ALIVE_100MS);
    wdg_m_main_100ms();
}

static sched_task_t s_tasks[] = {
    { "1ms",   task_1ms,   1U,   0U, 0U, 0U, 0U, 0U, 0U },
    { "10ms",  task_10ms,  10U,  1U, 0U, 0U, 0U, 0U, 0U },
    { "20ms",  task_20ms,  20U,  3U, 0U, 0U, 0U, 0U, 0U },
    { "100ms", task_100ms, 100U, 7U, 0U, 0U, 0U, 0U, 0U },
};

static const uds_cfg_t s_uds_cfg = { app_read_did, NVIC_SystemReset };

static void fatal_clock_failure(void)
{
    /* No reliable clock: no CAN, no UART timing. Fast LED blink, wait for power cycle. */
    gpio_clock_enable_all();
    gpio_config(LED_PORT, LED_PIN, GPIO_OUT_PP_2MHZ);
    for (;;)
    {
        gpio_toggle(LED_PORT, LED_PIN);
        for (volatile uint32_t i = 0UL; i < 100000UL; i++) { }
    }
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
    gpio_set(LED_PORT, LED_PIN);            /* LED off (active low) */
    systick_init(HCLK_HZ);

    uart_init(PCLK2_HZ, UART_BAUD);
    fault_enable_traps();
    const reset_cause_t rc = rcc_reset_cause();
    app_info_init(rc);
    log_printf("\r\nreset cause: %s\r\n", rcc_reset_cause_str(rc));

    (void)nvm_init();
    dem_init();
    if (rc == RESET_CAUSE_IWDG)
    {
        dem_report(DEM_EVT_WDG_RESET, true);    /* confirmed, persisted by dem_main_100ms */
        dem_report(DEM_EVT_WDG_RESET, false);
    }

    if (adc_init() != E_OK)
    {
        log_printf("ADC calibration timeout\r\n");
    }
    tim_backlight_init();
    tim_buzzer_init(2700UL);                /* typical piezo resonance */
    speed_sensor_init();
    buttons_init();

    spi_init();
    ili9341_init();
    dashboard_init();

    if (can_init(CAN_MODE_NORMAL) != E_OK)
    {
        log_printf("CAN init timeout: check transceiver and wiring (RX must idle high)\r\n");
    }
    com_init(tick_get());
    diag_init(&s_uds_cfg);
    cluster_init();
    cli_init();

    sched_init(s_tasks, (uint8_t)ARRAY_LEN(s_tasks));
    wdg_m_init(ALIVE_ALL);
    sched_run();
    return 0;
}
