/**
 * @file    can_if.c
 * @brief   TWAI interface (see can_if.h).
 */
#include "can_if.h"
#include "app_config.h"
#include "e2e.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "driver/twai.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "can_if";

#define CLUSTER_STATUS_TIMEOUT_US   (500000LL)

static volatile bool        s_logging;
static msg_cluster_status_t s_cluster;
static int64_t              s_cluster_rx_us;
static e2e_rx_state_t       s_cluster_e2e;
static uint32_t             s_cluster_e2e_errors;
static portMUX_TYPE         s_lock = portMUX_INITIALIZER_UNLOCKED;

static void log_frame(const twai_message_t *m, char dir)
{
    /* candump -L compatible: (seconds.micros) can0 ID#DATA */
    const int64_t t = esp_timer_get_time();
    char line[64];
    int n = snprintf(line, sizeof(line), "(%" PRId64 ".%06" PRId64 ") can0 %03" PRIX32 "#",
                     t / 1000000, t % 1000000, m->identifier);
    for (uint8_t i = 0; (i < m->data_length_code) && (n < (int)sizeof(line) - 3); i++)
    {
        n += snprintf(&line[n], sizeof(line) - (size_t)n, "%02X", m->data[i]);
    }
    printf("%s %c\n", line, dir);
}

static void on_cluster_status(const twai_message_t *m)
{
    if (m->data_length_code != DLC_CLUSTER_STATUS)
    {
        return;
    }
    const e2e_status_t st = e2e_check(CANID_CLUSTER_STATUS, m->data, DLC_CLUSTER_STATUS,
                                      CLUSTER_STATUS_CRC_POS, CLUSTER_STATUS_ALIVE_POS, &s_cluster_e2e);
    if (!e2e_status_usable(st))
    {
        s_cluster_e2e_errors++;
        return;
    }
    msg_cluster_status_t cs;
    can_unpack_cluster_status(m->data, &cs);
    portENTER_CRITICAL(&s_lock);
    s_cluster       = cs;
    s_cluster_rx_us = esp_timer_get_time();
    portEXIT_CRITICAL(&s_lock);
}

static void rx_task(void *arg)
{
    (void)arg;
    twai_message_t m;

    for (;;)
    {
        if (twai_receive(&m, portMAX_DELAY) != ESP_OK)
        {
            continue;
        }
        if (s_logging)
        {
            log_frame(&m, 'R');
        }
        if (m.extd || m.rtr)
        {
            continue;
        }
        switch (m.identifier)
        {
        case CANID_CLUSTER_STATUS:  on_cluster_status(&m);                              break;
        default:                                                                         break;
        }
    }
}

static void alert_task(void *arg)
{
    (void)arg;
    uint32_t alerts;

    for (;;)
    {
        if (twai_read_alerts(&alerts, portMAX_DELAY) != ESP_OK)
        {
            continue;
        }
        if (alerts & TWAI_ALERT_ERR_PASS)
        {
            ESP_LOGW(TAG, "error passive (is the STM32 node connected and ACKing?)");
        }
        if (alerts & TWAI_ALERT_BUS_OFF)
        {
            ESP_LOGE(TAG, "bus-off -> starting recovery (128 x 11 recessive bits)");
            twai_initiate_recovery();
        }
        if (alerts & TWAI_ALERT_BUS_RECOVERED)
        {
            ESP_LOGI(TAG, "bus recovered, restarting driver");
            twai_start();
        }
    }
}

void can_if_init(void)
{
    const twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(PIN_TWAI_TX, PIN_TWAI_RX, TWAI_MODE_NORMAL);
    /* 10 MHz quanta, 1 + 16 + 3 = 20 tq -> 500 kbit/s, sample point 85 %. The IDF default
     * macro samples at 80 %; this keeps both nodes close to the STM32's 83.3 %. */
    const twai_timing_config_t  t = {
        .clk_src = TWAI_CLK_SRC_DEFAULT, .quanta_resolution_hz = 10000000U, .brp = 0U,
        .tseg_1 = 16U, .tseg_2 = 3U, .sjw = 2U, .triple_sampling = false,
    };
    const twai_filter_config_t  f = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    ESP_ERROR_CHECK(twai_driver_install(&g, &t, &f));
    ESP_ERROR_CHECK(twai_start());
    ESP_ERROR_CHECK(twai_reconfigure_alerts(TWAI_ALERT_ERR_PASS | TWAI_ALERT_BUS_OFF |
                                            TWAI_ALERT_BUS_RECOVERED, NULL));

    xTaskCreate(rx_task, "can_rx", 4096, NULL, TASK_PRIO_CAN_RX, NULL);
    xTaskCreate(alert_task, "can_alert", 3072, NULL, TASK_PRIO_CAN_RX - 1, NULL);
    ESP_LOGI(TAG, "TWAI 500 kbit/s on TX=%d RX=%d", PIN_TWAI_TX, PIN_TWAI_RX);
}

bool can_if_send(uint32_t id, const uint8_t *data, uint8_t dlc)
{
    twai_message_t m = { 0 };
    m.identifier       = id;
    m.data_length_code = dlc;
    memcpy(m.data, data, dlc);

    /* 0 ticks: never block a periodic task; a full TX queue means the bus is down */
    const bool ok = (twai_transmit(&m, 0) == ESP_OK);
    if (ok && s_logging)
    {
        log_frame(&m, 'T');
    }
    return ok;
}

void can_if_set_logging(bool on)
{
    s_logging = on;
}

bool can_if_get_cluster_status(msg_cluster_status_t *out)
{
    portENTER_CRITICAL(&s_lock);
    *out = s_cluster;
    const int64_t rx = s_cluster_rx_us;
    portEXIT_CRITICAL(&s_lock);
    return (rx != 0) && ((esp_timer_get_time() - rx) < CLUSTER_STATUS_TIMEOUT_US);
}

void can_if_print_status(void)
{
    twai_status_info_t s;
    msg_cluster_status_t cs;

    if (twai_get_status_info(&s) == ESP_OK)
    {
        printf("TWAI state %d  TEC %" PRIu32 " REC %" PRIu32 "  tx_failed %" PRIu32
               " rx_missed %" PRIu32 " bus_err %" PRIu32 "\n",
               (int)s.state, s.tx_error_counter, s.rx_error_counter, s.tx_failed_count,
               s.rx_missed_count, s.bus_error_count);
    }
    if (can_if_get_cluster_status(&cs))
    {
        printf("Cluster: fuel %u%%  odo %" PRIu32 ".%" PRIu32 " km  warn 0x%02X  (E2E errors %" PRIu32 ")\n",
               cs.fuel_level_pct, cs.odometer_raw / 10U, cs.odometer_raw % 10U,
               cs.warning_bits, s_cluster_e2e_errors);
    }
    else
    {
        printf("Cluster: no ClusterStatus (0x300) in the last 500 ms\n");
    }
}
