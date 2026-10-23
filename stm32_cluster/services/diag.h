/**
 * @file    diag.h
 * @brief   Diagnostic stack glue: CAN <-> ISO-TP (0x7E0/0x7E8, functional 0x7DF) <-> UDS server.
 */
#ifndef DIAG_H
#define DIAG_H

#include "std_types.h"
#include "uds.h"
#include "isotp.h"

void diag_init(const uds_cfg_t *uds_cfg);
void diag_on_can_rx(const uint8_t *data, uint8_t dlc, bool functional, uint32_t now_ms);
void diag_main_1ms(uint32_t now_ms);
const isotp_stats_t *diag_isotp_stats(void);

#endif /* DIAG_H */
