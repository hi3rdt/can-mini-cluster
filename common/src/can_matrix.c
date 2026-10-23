/**
 * @file    can_matrix.c
 * @brief   Signal packing for the CAN matrix (see docs/can_matrix.dbc).
 *
 * Bit layout (Intel, start bit = LSB):
 *   EngineData    0x100: EngineSpeed 0|16, VehicleSpeed 16|16, CoolantTemp 32|8,
 *                        Alive 40|4, CRC 56|8   (byte 6 reserved = 0xFF)
 *   BodyStatus    0x200: TurnLeft 0|1, TurnRight 1|1, HighBeam 2|1, DoorOpen 3|1,
 *                        SeatbeltUnfastened 4|1, Alive 16|4, CRC 24|8
 *   ClusterStatus 0x300: FuelLevel 0|8, Odometer 8|24, WarningBits 32|8,
 *                        DtcCount 40|8, Alive 48|4, CRC 56|8
 */
#include "can_matrix.h"
#include <string.h>

void can_put_bits_le(uint8_t *buf, uint8_t start_bit, uint8_t len, uint32_t value)
{
    for (uint8_t i = 0U; i < len; i++)
    {
        const uint8_t pos  = (uint8_t)(start_bit + i);
        const uint8_t byte = (uint8_t)(pos >> 3);
        const uint8_t mask = (uint8_t)(1U << (pos & 7U));

        if (((value >> i) & 1UL) != 0UL)
        {
            buf[byte] |= mask;
        }
        else
        {
            buf[byte] &= (uint8_t)~mask;
        }
    }
}

uint32_t can_get_bits_le(const uint8_t *buf, uint8_t start_bit, uint8_t len)
{
    uint32_t value = 0UL;
    for (uint8_t i = 0U; i < len; i++)
    {
        const uint8_t pos = (uint8_t)(start_bit + i);
        if ((buf[pos >> 3] & (uint8_t)(1U << (pos & 7U))) != 0U)
        {
            value |= (1UL << i);
        }
    }
    return value;
}

void can_pack_engine_data(const msg_engine_data_t *m, uint8_t buf[DLC_ENGINE_DATA])
{
    (void)memset(buf, 0, DLC_ENGINE_DATA);
    buf[6] = 0xFFU;   /* reserved bytes are sent as 0xFF */
    can_put_bits_le(buf, 0U,  16U, m->engine_speed_raw);
    can_put_bits_le(buf, 16U, 16U, m->vehicle_speed_raw);
    can_put_bits_le(buf, 32U, 8U,  m->coolant_temp_raw);
    can_put_bits_le(buf, 40U, 4U,  m->alive);
    can_put_bits_le(buf, 56U, 8U,  m->crc);
}

void can_unpack_engine_data(const uint8_t buf[DLC_ENGINE_DATA], msg_engine_data_t *m)
{
    m->engine_speed_raw  = (uint16_t)can_get_bits_le(buf, 0U, 16U);
    m->vehicle_speed_raw = (uint16_t)can_get_bits_le(buf, 16U, 16U);
    m->coolant_temp_raw  = (uint8_t)can_get_bits_le(buf, 32U, 8U);
    m->alive             = (uint8_t)can_get_bits_le(buf, 40U, 4U);
    m->crc               = (uint8_t)can_get_bits_le(buf, 56U, 8U);
}

void can_pack_body_status(const msg_body_status_t *m, uint8_t buf[DLC_BODY_STATUS])
{
    (void)memset(buf, 0, DLC_BODY_STATUS);
    can_put_bits_le(buf, 0U,  1U, m->turn_left ? 1UL : 0UL);
    can_put_bits_le(buf, 1U,  1U, m->turn_right ? 1UL : 0UL);
    can_put_bits_le(buf, 2U,  1U, m->high_beam ? 1UL : 0UL);
    can_put_bits_le(buf, 3U,  1U, m->door_open ? 1UL : 0UL);
    can_put_bits_le(buf, 4U,  1U, m->seatbelt_unfastened ? 1UL : 0UL);
    can_put_bits_le(buf, 16U, 4U, m->alive);
    can_put_bits_le(buf, 24U, 8U, m->crc);
}

void can_unpack_body_status(const uint8_t buf[DLC_BODY_STATUS], msg_body_status_t *m)
{
    m->turn_left           = (can_get_bits_le(buf, 0U, 1U) != 0UL);
    m->turn_right          = (can_get_bits_le(buf, 1U, 1U) != 0UL);
    m->high_beam           = (can_get_bits_le(buf, 2U, 1U) != 0UL);
    m->door_open           = (can_get_bits_le(buf, 3U, 1U) != 0UL);
    m->seatbelt_unfastened = (can_get_bits_le(buf, 4U, 1U) != 0UL);
    m->alive               = (uint8_t)can_get_bits_le(buf, 16U, 4U);
    m->crc                 = (uint8_t)can_get_bits_le(buf, 24U, 8U);
}

void can_pack_cluster_status(const msg_cluster_status_t *m, uint8_t buf[DLC_CLUSTER_STATUS])
{
    (void)memset(buf, 0, DLC_CLUSTER_STATUS);
    can_put_bits_le(buf, 0U,  8U,  m->fuel_level_pct);
    can_put_bits_le(buf, 8U,  24U, m->odometer_raw & ODO_RAW_MAX);
    can_put_bits_le(buf, 32U, 8U,  m->warning_bits);
    can_put_bits_le(buf, 40U, 8U,  m->dtc_count);
    can_put_bits_le(buf, 48U, 4U,  m->alive);
    can_put_bits_le(buf, 56U, 8U,  m->crc);
}

void can_unpack_cluster_status(const uint8_t buf[DLC_CLUSTER_STATUS], msg_cluster_status_t *m)
{
    m->fuel_level_pct = (uint8_t)can_get_bits_le(buf, 0U, 8U);
    m->odometer_raw   = can_get_bits_le(buf, 8U, 24U);
    m->warning_bits   = (uint8_t)can_get_bits_le(buf, 32U, 8U);
    m->dtc_count      = (uint8_t)can_get_bits_le(buf, 40U, 8U);
    m->alive          = (uint8_t)can_get_bits_le(buf, 48U, 4U);
    m->crc            = (uint8_t)can_get_bits_le(buf, 56U, 8U);
}
