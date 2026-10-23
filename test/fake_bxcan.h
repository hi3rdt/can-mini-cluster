/**
 * @file    fake_bxcan.h
 * @brief   Host stand-in for the bxCAN driver: tests push RX frames and inspect TX frames.
 */
#ifndef FAKE_BXCAN_H
#define FAKE_BXCAN_H

#include "bxcan.h"

void fake_can_reset(void);
void fake_can_push_rx(uint32_t id, const uint8_t *data, uint8_t dlc);
/** Last frame passed to can_send(); returns the number of frames sent so far. */
uint32_t fake_can_last_tx(can_frame_t *out);

#endif /* FAKE_BXCAN_H */
