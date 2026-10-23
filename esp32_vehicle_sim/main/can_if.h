/**
 * @file    can_if.h
 * @brief   TWAI (CAN) interface: driver setup, RX dispatch, bus-off recovery, candump logging.
 */
#ifndef CAN_IF_H
#define CAN_IF_H

#include <stdbool.h>
#include <stdint.h>
#include "can_matrix.h"

void can_if_init(void);
bool can_if_send(uint32_t id, const uint8_t *data, uint8_t dlc);
void can_if_set_logging(bool on);
/** Latest ClusterStatus from the STM32; returns false if never received / stale (> 500 ms). */
bool can_if_get_cluster_status(msg_cluster_status_t *out);
void can_if_print_status(void);

#endif /* CAN_IF_H */
