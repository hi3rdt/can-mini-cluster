/**
 * @file    com.c
 * @brief   COM layer (see com.h).
 *
 * A message is "valid" while E2E-correct frames keep arriving within its timeout
 * (5 x cycle time). The timeout is restarted only by a frame that passed the E2E
 * check, so a sender that keeps transmitting corrupted data still ends in "lost".
 */
#include "com.h"
#include "bxcan.h"
#include "e2e.h"
#include <string.h>

#define ENGINE_TIMEOUT_MS   (CYCLE_ENGINE_DATA_MS * TIMEOUT_FACTOR)    /* 100 ms */
#define BODY_TIMEOUT_MS     (CYCLE_BODY_STATUS_MS * TIMEOUT_FACTOR)    /* 500 ms */
#define MAX_FRAMES_PER_CALL (8U)    /* bounds the time spent in one call */

typedef struct
{
    uint32_t       last_ok_ms;  /* time of the last E2E-correct frame */
    bool           valid;
    e2e_rx_state_t e2e;
} rx_monitor_t;

static msg_engine_data_t s_engine;
static msg_body_status_t s_body;
static rx_monitor_t      s_mon_engine;
static rx_monitor_t      s_mon_body;
static com_stats_t       s_stats;
static uint8_t           s_tx_alive;

static bool accept_e2e(rx_monitor_t *mon, uint16_t id, const uint8_t *d, uint8_t len,
                       uint8_t crc_pos, uint8_t alive_pos, uint32_t now)
{
    const e2e_status_t st = e2e_check(id, d, len, crc_pos, alive_pos, &mon->e2e);

    if (st == E2E_STATUS_CRC_ERROR)
    {
        s_stats.e2e_crc_errors++;
    }
    else if ((st == E2E_STATUS_WRONG_SEQUENCE) || (st == E2E_STATUS_REPEATED))
    {
        s_stats.e2e_seq_errors++;
    }
    else
    {
        /* usable */
    }

    if (!e2e_status_usable(st))
    {
        return false;
    }
    mon->last_ok_ms = now;
    mon->valid      = true;
    return true;
}

static void on_engine(const can_frame_t *f, uint32_t now)
{
    if (f->dlc != DLC_ENGINE_DATA)
    {
        return;
    }
    if (accept_e2e(&s_mon_engine, CANID_ENGINE_DATA, f->data, DLC_ENGINE_DATA,
                   ENGINE_DATA_CRC_POS, ENGINE_DATA_ALIVE_POS, now))
    {
        can_unpack_engine_data(f->data, &s_engine);
        s_stats.engine_rx++;
    }
}

static void on_body(const can_frame_t *f, uint32_t now)
{
    if (f->dlc != DLC_BODY_STATUS)
    {
        return;
    }
    if (accept_e2e(&s_mon_body, CANID_BODY_STATUS, f->data, DLC_BODY_STATUS,
                   BODY_STATUS_CRC_POS, BODY_STATUS_ALIVE_POS, now))
    {
        can_unpack_body_status(f->data, &s_body);
        s_stats.body_rx++;
    }
}

void com_init(uint32_t now_ms)
{
    (void)now_ms;
    (void)memset(&s_engine, 0, sizeof(s_engine));
    (void)memset(&s_body, 0, sizeof(s_body));
    (void)memset(&s_mon_engine, 0, sizeof(s_mon_engine));
    (void)memset(&s_mon_body, 0, sizeof(s_mon_body));
    (void)memset(&s_stats, 0, sizeof(s_stats));
    s_tx_alive = 0U;
}

void com_poll_rx(uint32_t now_ms)
{
    can_frame_t f;
    uint8_t n = 0U;

    while ((n < MAX_FRAMES_PER_CALL) && can_receive(&f))
    {
        n++;
        switch (f.id)
        {
        case CANID_ENGINE_DATA: on_engine(&f, now_ms); break;
        case CANID_BODY_STATUS: on_body(&f, now_ms);   break;
        default:                                       break;  /* filtered in hardware */
        }
    }
}

static void monitor(rx_monitor_t *mon, uint32_t timeout_ms, uint32_t *timeouts, uint32_t now)
{
    /* Unsigned subtraction stays correct when the 32-bit ms counter wraps */
    if (mon->valid && ((uint32_t)(now - mon->last_ok_ms) >= timeout_ms))
    {
        mon->valid = false;
        mon->e2e.synced = false;    /* sender restarted: accept its first counter value */
        (*timeouts)++;
    }
}

void com_check_timeouts(uint32_t now_ms)
{
    monitor(&s_mon_engine, ENGINE_TIMEOUT_MS, &s_stats.engine_timeouts, now_ms);
    monitor(&s_mon_body, BODY_TIMEOUT_MS, &s_stats.body_timeouts, now_ms);
}

std_return_t com_send_cluster_status(const msg_cluster_status_t *m)
{
    uint8_t buf[DLC_CLUSTER_STATUS];
    can_pack_cluster_status(m, buf);
    e2e_protect(CANID_CLUSTER_STATUS, buf, DLC_CLUSTER_STATUS, CLUSTER_STATUS_CRC_POS,
                CLUSTER_STATUS_ALIVE_POS, &s_tx_alive);
    return can_send(CANID_CLUSTER_STATUS, buf, DLC_CLUSTER_STATUS);
}

bool com_get_engine(msg_engine_data_t *out)
{
    *out = s_engine;
    return s_mon_engine.valid;
}

bool com_get_body(msg_body_status_t *out)
{
    *out = s_body;
    return s_mon_body.valid;
}

void com_get_stats(com_stats_t *out)
{
    *out = s_stats;
}
