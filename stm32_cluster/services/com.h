/**
 * @file    com.h
 * @brief   COM layer: CAN RX dispatch, E2E check, deadline monitoring, ClusterStatus TX.
 */
#ifndef COM_H
#define COM_H

#include "std_types.h"
#include "can_matrix.h"

#define COM_E2E_FAIL_THRESHOLD  (3U)    /* consecutive bad frames before DTC U0401 */

typedef struct
{
    uint32_t engine_rx;
    uint32_t body_rx;
    uint32_t e2e_crc_errors;
    uint32_t e2e_seq_errors;
    uint32_t engine_timeouts;
    uint32_t body_timeouts;
} com_stats_t;

void com_init(uint32_t now_ms);
void com_main_1ms(uint32_t now_ms);     /* drain RX queue */
void com_main_10ms(uint32_t now_ms);    /* deadline monitoring + DEM */
std_return_t com_send_cluster_status(const msg_cluster_status_t *m);

/** Copy latest signals; returns true if the message is within its deadline. */
bool com_get_engine(msg_engine_data_t *out);
bool com_get_body(msg_body_status_t *out);
void com_get_stats(com_stats_t *out);

#endif /* COM_H */
