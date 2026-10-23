/**
 * @file    fake_bxcan.c
 * @brief   Host stand-in for the bxCAN driver (see fake_bxcan.h).
 */
#include "fake_bxcan.h"
#include <string.h>

static can_frame_t s_rx[CAN_RXQ_SIZE];
static uint32_t    s_rx_head;
static uint32_t    s_rx_tail;
static can_frame_t s_tx_last;
static uint32_t    s_tx_count;

void fake_can_reset(void)
{
    s_rx_head  = 0U;
    s_rx_tail  = 0U;
    s_tx_count = 0U;
    (void)memset(&s_tx_last, 0, sizeof(s_tx_last));
}

void fake_can_push_rx(uint32_t id, const uint8_t *data, uint8_t dlc)
{
    can_frame_t *f = &s_rx[s_rx_head % CAN_RXQ_SIZE];
    f->id  = id;
    f->dlc = dlc;
    (void)memcpy(f->data, data, dlc);
    s_rx_head++;
}

uint32_t fake_can_last_tx(can_frame_t *out)
{
    *out = s_tx_last;
    return s_tx_count;
}

bool can_receive(can_frame_t *f)
{
    if (s_rx_tail == s_rx_head)
    {
        return false;
    }
    *f = s_rx[s_rx_tail % CAN_RXQ_SIZE];
    s_rx_tail++;
    return true;
}

std_return_t can_send(uint32_t id, const uint8_t *data, uint8_t dlc)
{
    s_tx_last.id  = id;
    s_tx_last.dlc = dlc;
    (void)memcpy(s_tx_last.data, data, dlc);
    s_tx_count++;
    return E_OK;
}
