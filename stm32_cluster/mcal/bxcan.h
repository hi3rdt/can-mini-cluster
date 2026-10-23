/**
 * @file    bxcan.h
 * @brief   bxCAN driver: 500 kbit/s on PB8/PB9, hardware ID-list filter, RX FIFO0 -> SW queue.
 */
#ifndef BXCAN_H
#define BXCAN_H

#include "std_types.h"

#define CAN_RXQ_SIZE    (32U)       /* power of two */

typedef struct
{
    uint32_t id;
    uint8_t  dlc;
    uint8_t  data[8];
} can_frame_t;

typedef struct
{
    uint32_t rx_frames;
    uint32_t rx_queue_overflows;    /* software queue full          */
    uint32_t rx_fifo_overruns;      /* hardware FIFO0 overrun (FOVR) */
    uint32_t tx_frames;
    uint32_t tx_no_mailbox;         /* all 3 mailboxes busy          */
    uint32_t bus_off_events;
    uint8_t  tec;                   /* transmit error counter        */
    uint8_t  rec;                   /* receive error counter         */
    uint8_t  last_error_code;       /* ESR.LEC                       */
    bool     error_passive;
    bool     bus_off;
} can_stats_t;

typedef enum
{
    CAN_MODE_NORMAL = 0,
    CAN_MODE_LOOPBACK_SILENT        /* self-test without transceiver */
} can_mode_t;

std_return_t can_init(can_mode_t mode);
std_return_t can_send(uint32_t id, const uint8_t *data, uint8_t dlc);
bool         can_receive(can_frame_t *f);            /* main context, false if empty */
void         can_get_stats(can_stats_t *out);
/** True once after each bus-off entry (consumed by the caller). */
bool         can_take_busoff_event(void);

#endif /* BXCAN_H */
