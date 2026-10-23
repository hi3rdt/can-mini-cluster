/**
 * @file    tester.h
 * @brief   UDS tester (client) over ISO-TP 0x7E0 -> 0x7E8, using the shared isotp.c.
 */
#ifndef TESTER_H
#define TESTER_H

#include <stdbool.h>
#include <stdint.h>

#define TESTER_MAX_REQ  (64U)

void tester_init(void);
/** Queue a request (called from the console task). */
bool tester_request(const uint8_t *req, uint16_t len);
/** CAN RX hook for 0x7E8 frames (called from the CAN RX task). */
void tester_on_can_rx(const uint8_t *data, uint8_t dlc);
/** Periodic TesterPresent (0x3E 0x80) every 2 s, keeps the extended session alive. */
void tester_keepalive(bool on);

#endif /* TESTER_H */
