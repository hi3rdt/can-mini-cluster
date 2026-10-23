/**
 * @file    e2e.h
 * @brief   Lightweight end-to-end protection: CRC8 SAE J1850 + 4-bit alive counter.
 *
 * Loosely modelled on AUTOSAR E2E Profile 1: the CRC covers the 16-bit
 * Data ID (CAN identifier, low byte first) followed by every payload byte
 * except the CRC byte itself.
 */
#ifndef E2E_H
#define E2E_H

#include "std_types.h"

#define E2E_ALIVE_MAX       (0x0FU)  /* counter wraps 15 -> 0 */
#define E2E_MAX_DELTA       (2U)     /* accept up to 1 lost frame */

typedef enum
{
    E2E_STATUS_OK = 0,          /* counter +1, CRC valid                 */
    E2E_STATUS_OK_SOME_LOST,    /* counter +2..MAX_DELTA, CRC valid      */
    E2E_STATUS_INITIAL,         /* first frame after init/resync         */
    E2E_STATUS_REPEATED,        /* same counter as last frame            */
    E2E_STATUS_WRONG_SEQUENCE,  /* counter jump > MAX_DELTA              */
    E2E_STATUS_CRC_ERROR        /* CRC mismatch, data must not be used   */
} e2e_status_t;

typedef struct
{
    uint8_t last_counter;
    bool    synced;
} e2e_rx_state_t;

/**
 * CRC8 SAE J1850: poly 0x1D, init 0xFF, final XOR 0xFF (check value "123456789" = 0x4B).
 * @param crc_in  0x00 for a fresh CRC, or a previous result to continue it.
 */
uint8_t e2e_crc8(const uint8_t *data, uint32_t len, uint8_t crc_in);

/** CRC over Data ID + payload with the CRC byte skipped. */
uint8_t e2e_compute(uint16_t data_id, const uint8_t *buf, uint8_t len, uint8_t crc_pos);

/**
 * Write alive counter (low nibble of buf[alive_pos]) and CRC into buf.
 * @param counter  in/out: incremented modulo 16 after use.
 */
void e2e_protect(uint16_t data_id, uint8_t *buf, uint8_t len,
                 uint8_t crc_pos, uint8_t alive_pos, uint8_t *counter);

/** Verify a received frame and update the receiver state. */
e2e_status_t e2e_check(uint16_t data_id, const uint8_t *buf, uint8_t len,
                       uint8_t crc_pos, uint8_t alive_pos, e2e_rx_state_t *state);

/** True when the status means the data may be used by the application. */
static inline bool e2e_status_usable(e2e_status_t st)
{
    return (st == E2E_STATUS_OK) || (st == E2E_STATUS_OK_SOME_LOST) ||
           (st == E2E_STATUS_INITIAL);
}

#endif /* E2E_H */
