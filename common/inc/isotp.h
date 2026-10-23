/**
 * @file    isotp.h
 * @brief   ISO 15765-2 transport layer for classic CAN (8-byte frames, normal addressing).
 *
 * One isotp_t instance = one full-duplex channel (rx_id/tx_id pair).
 * No dynamic memory and no OS dependency: the caller feeds received frames,
 * calls isotp_main() periodically (1 ms recommended) and supplies a CAN TX hook.
 * Used both by the STM32 diagnostic server and by the ESP32 tester.
 */
#ifndef ISOTP_H
#define ISOTP_H

#include "std_types.h"

#define ISOTP_MAX_PAYLOAD   (4095U)     /* 12-bit FF_DL */
#define ISOTP_N_BS_MS       (1000U)     /* sender waits for FlowControl */
#define ISOTP_N_CR_MS       (1000U)     /* receiver waits for ConsecutiveFrame */
#define ISOTP_PADDING       (0xCCU)

typedef void (*isotp_can_tx_fn)(uint32_t can_id, const uint8_t data[8], void *ctx);
typedef void (*isotp_rx_done_fn)(const uint8_t *data, uint16_t len, void *ctx);

typedef struct
{
    uint32_t         tx_id;
    uint32_t         rx_id;
    uint8_t          block_size;    /* BS we announce in our FlowControl (0 = no limit) */
    uint8_t          st_min_ms;     /* STmin we announce, 0..127 ms */
    uint8_t         *rx_buf;
    uint16_t         rx_buf_size;
    uint8_t         *tx_buf;
    uint16_t         tx_buf_size;
    isotp_can_tx_fn  can_tx;
    isotp_rx_done_fn rx_done;
    void            *ctx;
} isotp_cfg_t;

typedef enum
{
    ISOTP_TX_IDLE = 0,
    ISOTP_TX_WAIT_FC,
    ISOTP_TX_SEND_CF
} isotp_tx_state_t;

typedef enum
{
    ISOTP_RX_IDLE = 0,
    ISOTP_RX_WAIT_CF
} isotp_rx_state_t;

typedef struct
{
    uint16_t rx_ok;
    uint16_t tx_ok;
    uint16_t rx_errors;     /* wrong SN, N_Cr timeout, overflow */
    uint16_t tx_errors;     /* N_Bs timeout, FC overflow        */
} isotp_stats_t;

typedef struct
{
    const isotp_cfg_t *cfg;

    isotp_tx_state_t tx_state;
    uint16_t tx_len;
    uint16_t tx_offset;
    uint8_t  tx_sn;
    uint8_t  tx_bs_left;        /* CFs left in the current block, 0 = unlimited */
    bool     tx_bs_limited;
    uint8_t  tx_st_min_ms;
    uint32_t tx_timer_ms;       /* N_Bs deadline or next CF time */

    isotp_rx_state_t rx_state;
    uint16_t rx_len;
    uint16_t rx_offset;
    uint8_t  rx_sn;
    uint8_t  rx_bs_count;
    uint32_t rx_timer_ms;       /* N_Cr deadline */

    isotp_stats_t stats;
} isotp_t;

void         isotp_init(isotp_t *ch, const isotp_cfg_t *cfg);
void         isotp_on_can_rx(isotp_t *ch, const uint8_t *data, uint8_t dlc, uint32_t now_ms);
std_return_t isotp_send(isotp_t *ch, const uint8_t *data, uint16_t len, uint32_t now_ms);
void         isotp_main(isotp_t *ch, uint32_t now_ms);
bool         isotp_tx_busy(const isotp_t *ch);

#endif /* ISOTP_H */
