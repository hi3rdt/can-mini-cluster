/**
 * @file    diag.c
 * @brief   Diagnostic stack glue (see diag.h).
 */
#include "diag.h"
#include "bxcan.h"
#include "can_matrix.h"

#define DIAG_BUF_SIZE   (128U)

static uint8_t     s_rx_buf[DIAG_BUF_SIZE];
static uint8_t     s_tx_buf[DIAG_BUF_SIZE];
static uint8_t     s_resp[DIAG_BUF_SIZE];
static isotp_t     s_tp;
static uint32_t    s_now;

static void tp_can_tx(uint32_t can_id, const uint8_t data[8], void *ctx)
{
    (void)ctx;
    (void)can_send(can_id, data, 8U);       /* TX mailbox full -> frame lost, tester retries */
}

static void respond(const uint8_t *req, uint16_t len, bool functional)
{
    const uint16_t n = uds_process(req, len, functional, s_resp, sizeof(s_resp), s_now);
    if (n > 0U)
    {
        (void)isotp_send(&s_tp, s_resp, n, s_now);
    }
}

static void tp_rx_done(const uint8_t *data, uint16_t len, void *ctx)
{
    (void)ctx;
    respond(data, len, false);
}

static const isotp_cfg_t s_tp_cfg = {
    CANID_DIAG_PHYS_RESP,   /* tx_id */
    CANID_DIAG_PHYS_REQ,    /* rx_id */
    0U,                     /* BS: tester may send all CFs */
    1U,                     /* STmin 1 ms: one CF per 1 ms task is enough */
    s_rx_buf, DIAG_BUF_SIZE,
    s_tx_buf, DIAG_BUF_SIZE,
    tp_can_tx,
    tp_rx_done,
    NULL
};

void diag_init(const uds_cfg_t *uds_cfg)
{
    isotp_init(&s_tp, &s_tp_cfg);
    uds_init(uds_cfg);
}

void diag_on_can_rx(const uint8_t *data, uint8_t dlc, bool functional, uint32_t now_ms)
{
    s_now = now_ms;
    if (functional)
    {
        /* Functional addressing is single-frame only (ISO 15765-2 9.6.2) */
        const uint8_t sf_dl = (uint8_t)(data[0] & 0x0FU);
        if (((data[0] >> 4) == 0U) && (sf_dl > 0U) && (sf_dl < dlc))
        {
            respond(&data[1], sf_dl, true);
        }
    }
    else
    {
        isotp_on_can_rx(&s_tp, data, dlc, now_ms);
    }
}

void diag_main_1ms(uint32_t now_ms)
{
    s_now = now_ms;
    isotp_main(&s_tp, now_ms);
    uds_main(now_ms);
}

const isotp_stats_t *diag_isotp_stats(void)
{
    return &s_tp.stats;
}
