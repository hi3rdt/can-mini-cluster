/**
 * @file    e2e.c
 * @brief   CRC8 SAE J1850 + alive counter protection (see e2e.h).
 */
#include "e2e.h"

#define CRC8_J1850_POLY     (0x1DU)
#define CRC8_J1850_INIT     (0xFFU)
#define CRC8_J1850_XOROUT   (0xFFU)

/* Bitwise implementation: 8 frames/100 ms on this bus, a 256-byte table is not worth the flash */
static uint8_t crc8_update(uint8_t crc, uint8_t byte)
{
    uint8_t c = (uint8_t)(crc ^ byte);
    for (uint8_t bit = 0U; bit < 8U; bit++)
    {
        if ((c & 0x80U) != 0U)
        {
            c = (uint8_t)((uint8_t)(c << 1) ^ CRC8_J1850_POLY);
        }
        else
        {
            c = (uint8_t)(c << 1);
        }
    }
    return c;
}

uint8_t e2e_crc8(const uint8_t *data, uint32_t len, uint8_t crc_in)
{
    uint8_t crc = (uint8_t)(crc_in ^ CRC8_J1850_XOROUT);   /* allows chaining */
    for (uint32_t i = 0U; i < len; i++)
    {
        crc = crc8_update(crc, data[i]);
    }
    return (uint8_t)(crc ^ CRC8_J1850_XOROUT);
}

uint8_t e2e_compute(uint16_t data_id, const uint8_t *buf, uint8_t len, uint8_t crc_pos)
{
    uint8_t crc = CRC8_J1850_INIT;

    crc = crc8_update(crc, (uint8_t)(data_id & 0xFFU));
    crc = crc8_update(crc, (uint8_t)(data_id >> 8));
    for (uint8_t i = 0U; i < len; i++)
    {
        if (i != crc_pos)
        {
            crc = crc8_update(crc, buf[i]);
        }
    }
    return (uint8_t)(crc ^ CRC8_J1850_XOROUT);
}

void e2e_protect(uint16_t data_id, uint8_t *buf, uint8_t len,
                 uint8_t crc_pos, uint8_t alive_pos, uint8_t *counter)
{
    buf[alive_pos] = (uint8_t)((buf[alive_pos] & 0xF0U) | (*counter & E2E_ALIVE_MAX));
    buf[crc_pos]   = e2e_compute(data_id, buf, len, crc_pos);
    *counter       = (uint8_t)((*counter + 1U) & E2E_ALIVE_MAX);
}

e2e_status_t e2e_check(uint16_t data_id, const uint8_t *buf, uint8_t len,
                       uint8_t crc_pos, uint8_t alive_pos, e2e_rx_state_t *state)
{
    e2e_status_t st;

    if (e2e_compute(data_id, buf, len, crc_pos) != buf[crc_pos])
    {
        return E2E_STATUS_CRC_ERROR;   /* state untouched: a corrupted counter is meaningless */
    }

    const uint8_t counter = (uint8_t)(buf[alive_pos] & E2E_ALIVE_MAX);

    if (!state->synced)
    {
        st = E2E_STATUS_INITIAL;
        state->synced = true;
    }
    else
    {
        const uint8_t delta = (uint8_t)((counter - state->last_counter) & E2E_ALIVE_MAX);
        if (delta == 0U)
        {
            st = E2E_STATUS_REPEATED;
        }
        else if (delta == 1U)
        {
            st = E2E_STATUS_OK;
        }
        else if (delta <= E2E_MAX_DELTA)
        {
            st = E2E_STATUS_OK_SOME_LOST;
        }
        else
        {
            st = E2E_STATUS_WRONG_SEQUENCE;
        }
    }

    state->last_counter = counter;  /* resync on wrong sequence: next +1 is OK again */
    return st;
}
