/**
 * @file    tester.c
 * @brief   UDS tester (see tester.h).
 *
 * One task owns the ISO-TP channel (no locking inside isotp.c): CAN RX frames and
 * console requests reach it through queues, and it calls isotp_main() every 1 ms.
 */
#include "tester.h"
#include "app_config.h"
#include "can_if.h"
#include "can_matrix.h"
#include "isotp.h"

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define RESP_BUF_SIZE       (256U)
#define P2_STAR_TIMEOUT_MS  (5000U)
#define KEEPALIVE_MS        (2000U)

typedef struct { uint8_t data[8]; uint8_t dlc; } rx_frame_t;
typedef struct { uint8_t data[TESTER_MAX_REQ]; uint16_t len; } request_t;

static QueueHandle_t s_rx_q;
static QueueHandle_t s_req_q;
static uint8_t  s_rx_buf[RESP_BUF_SIZE];
static uint8_t  s_tx_buf[TESTER_MAX_REQ];
static isotp_t  s_tp;
static bool     s_waiting;
static uint32_t s_deadline_ms;
static uint8_t  s_pending_sid;
static volatile bool s_keepalive;

static uint32_t now_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static const char *nrc_name(uint8_t nrc)
{
    switch (nrc)
    {
    case 0x10: return "generalReject";
    case 0x11: return "serviceNotSupported";
    case 0x12: return "subFunctionNotSupported";
    case 0x13: return "incorrectMessageLengthOrInvalidFormat";
    case 0x14: return "responseTooLong";
    case 0x22: return "conditionsNotCorrect";
    case 0x31: return "requestOutOfRange";
    case 0x78: return "responsePending";
    case 0x7F: return "serviceNotSupportedInActiveSession";
    default:   return "?";
    }
}

static void print_response(const uint8_t *d, uint16_t len)
{
    printf("UDS <- ");
    for (uint16_t i = 0; i < len; i++)
    {
        printf("%02X ", d[i]);
    }
    printf("\n");

    if ((len >= 3U) && (d[0] == 0x7FU))
    {
        printf("       negative response to 0x%02X: NRC 0x%02X %s\n", d[1], d[2], nrc_name(d[2]));
    }
    else if ((len >= 3U) && (d[0] == 0x62U))
    {
        const uint16_t did = (uint16_t)((d[1] << 8) | d[2]);
        if ((did == 0xF190U) || (did == 0xF195U))
        {
            printf("       DID %04X = \"%.*s\"\n", did, (int)(len - 3U), (const char *)&d[3]);
        }
    }
    else if ((len >= 3U) && (d[0] == 0x59U) && (d[1] == 0x02U))
    {
        printf("       %u DTC(s), availability mask 0x%02X\n", (unsigned)((len - 3U) / 4U), d[2]);
        for (uint16_t i = 3U; (i + 3U) < len; i += 4U)
        {
            const char area[4] = { 'P', 'C', 'B', 'U' };
            const uint16_t code = (uint16_t)(((d[i] & 0x3FU) << 8) | d[i + 1U]);
            printf("       %c%04X-%02X  status 0x%02X%s\n", area[d[i] >> 6], code, d[i + 2U],
                   d[i + 3U], (d[i + 3U] & 0x08U) ? " confirmed" : "");
        }
    }
    else
    {
        /* raw dump is enough */
    }
}

static bool has_subfunction(uint8_t sid)
{
    return (sid == 0x10U) || (sid == 0x11U) || (sid == 0x19U) || (sid == 0x27U) ||
           (sid == 0x28U) || (sid == 0x31U) || (sid == 0x3EU) || (sid == 0x85U);
}

static void tp_can_tx(uint32_t can_id, const uint8_t data[8], void *ctx)
{
    (void)ctx;
    (void)can_if_send(can_id, data, 8U);
}

static void tp_rx_done(const uint8_t *data, uint16_t len, void *ctx)
{
    (void)ctx;
    if ((len >= 3U) && (data[0] == 0x7FU) && (data[2] == 0x78U))
    {
        s_deadline_ms = now_ms() + P2_STAR_TIMEOUT_MS;   /* server needs more time */
        return;
    }
    const bool is_keepalive_echo = (len == 2U) && (data[0] == 0x7EU);
    if (!is_keepalive_echo)
    {
        print_response(data, len);
    }
    s_waiting = false;
}

static const isotp_cfg_t s_tp_cfg = {
    .tx_id = CANID_DIAG_PHYS_REQ, .rx_id = CANID_DIAG_PHYS_RESP,
    .block_size = 8U, .st_min_ms = 0U,
    .rx_buf = s_rx_buf, .rx_buf_size = RESP_BUF_SIZE,
    .tx_buf = s_tx_buf, .tx_buf_size = TESTER_MAX_REQ,
    .can_tx = tp_can_tx, .rx_done = tp_rx_done, .ctx = NULL,
};

static void tester_task(void *arg)
{
    (void)arg;
    rx_frame_t f;
    request_t r;
    uint32_t last_keepalive = 0U;

    for (;;)
    {
        while (xQueueReceive(s_rx_q, &f, 0) == pdTRUE)
        {
            isotp_on_can_rx(&s_tp, f.data, f.dlc, now_ms());
        }
        const uint32_t now = now_ms();
        isotp_main(&s_tp, now);

        if (s_waiting && ((int32_t)(now - s_deadline_ms) >= 0))
        {
            if (s_pending_sid != 0x3EU)
            {
                printf("UDS: no response to 0x%02X within P2* (is the cluster on the bus?)\n", s_pending_sid);
            }
            s_waiting = false;
        }

        if (!s_waiting && !isotp_tx_busy(&s_tp))
        {
            bool have = (xQueueReceive(s_req_q, &r, 0) == pdTRUE);
            if (!have && s_keepalive && ((now - last_keepalive) >= KEEPALIVE_MS))
            {
                r.data[0] = 0x3EU;
                r.data[1] = 0x00U;      /* with response, so the tester notices a dead server */
                r.len = 2U;
                have = true;
            }
            if (have)
            {
                if (r.data[0] == 0x3EU)
                {
                    last_keepalive = now;
                }
                if (isotp_send(&s_tp, r.data, r.len, now) == E_OK)
                {
                    /* suppressPosRspMsgIndicationBit only exists for sub-function services */
                    s_waiting     = !(has_subfunction(r.data[0]) && (r.len >= 2U) &&
                                      ((r.data[1] & 0x80U) != 0U));
                    s_pending_sid = r.data[0];
                    s_deadline_ms = now + P2_STAR_TIMEOUT_MS;
                }
            }
        }
        vTaskDelay(1);
    }
}

void tester_init(void)
{
    s_rx_q  = xQueueCreate(32, sizeof(rx_frame_t));
    s_req_q = xQueueCreate(4, sizeof(request_t));
    isotp_init(&s_tp, &s_tp_cfg);
    xTaskCreate(tester_task, "tester", 4096, NULL, TASK_PRIO_TESTER, NULL);
}

bool tester_request(const uint8_t *req, uint16_t len)
{
    request_t r;
    if ((len == 0U) || (len > TESTER_MAX_REQ))
    {
        return false;
    }
    memcpy(r.data, req, len);
    r.len = len;
    return xQueueSend(s_req_q, &r, 0) == pdTRUE;
}

void tester_on_can_rx(const uint8_t *data, uint8_t dlc)
{
    rx_frame_t f;
    memcpy(f.data, data, 8);
    f.dlc = dlc;
    (void)xQueueSend(s_rx_q, &f, 0);
}

void tester_keepalive(bool on)
{
    s_keepalive = on;
}
