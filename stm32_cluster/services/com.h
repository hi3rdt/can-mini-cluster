/**
 * @file    com.h
 * @brief   COM layer: CAN RX dispatch, E2E check, timeout monitoring, ClusterStatus TX.
 */
#ifndef COM_H
#define COM_H

#include "std_types.h"
#include "can_matrix.h"

typedef struct
{
    uint32_t engine_rx;         /* frames accepted (E2E OK)            */
    uint32_t body_rx;
    uint32_t e2e_crc_errors;    /* frames dropped: wrong CRC           */
    uint32_t e2e_seq_errors;    /* frames dropped: repeated / jumped counter */
    uint32_t engine_timeouts;   /* valid -> lost transitions           */
    uint32_t body_timeouts;
} com_stats_t;

void com_init(uint32_t now_ms);
/** Drain the CAN RX queue (call as often as possible, at least every 1 ms). */
void com_poll_rx(uint32_t now_ms);
/** Timeout monitoring (call every 10 ms). */
void com_check_timeouts(uint32_t now_ms);
std_return_t com_send_cluster_status(const msg_cluster_status_t *m);

/** Copy the latest signals; returns true if the message is valid (not timed out). */
bool com_get_engine(msg_engine_data_t *out);
bool com_get_body(msg_body_status_t *out);
void com_get_stats(com_stats_t *out);

#endif /* COM_H */
