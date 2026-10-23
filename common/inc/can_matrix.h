/**
 * @file    can_matrix.h
 * @brief   CAN communication matrix shared by the STM32 cluster and the ESP32 simulator.
 *
 * Single source of truth is docs/can_matrix.dbc; this header mirrors it by hand and
 * test/test_can_matrix.c checks both against vectors generated with cantools.
 * All signals are Intel (little-endian) byte order, 500 kbit/s, 11-bit identifiers.
 */
#ifndef CAN_MATRIX_H
#define CAN_MATRIX_H

#include "std_types.h"

/* ---- Identifiers ------------------------------------------------------- */
#define CANID_ENGINE_DATA       (0x100U)
#define CANID_BODY_STATUS       (0x200U)
#define CANID_CLUSTER_STATUS    (0x300U)
#define CANID_DIAG_FUNC_REQ     (0x7DFU)
#define CANID_DIAG_PHYS_REQ     (0x7E0U)
#define CANID_DIAG_PHYS_RESP    (0x7E8U)

/* ---- Timing ------------------------------------------------------------ */
#define CYCLE_ENGINE_DATA_MS    (20U)
#define CYCLE_BODY_STATUS_MS    (100U)
#define CYCLE_CLUSTER_STATUS_MS (100U)
#define TIMEOUT_FACTOR          (5U)    /* message lost after 5 missed cycles */

/* ---- Lengths and E2E positions ---------------------------------------- */
#define DLC_ENGINE_DATA         (8U)
#define DLC_BODY_STATUS         (4U)
#define DLC_CLUSTER_STATUS      (8U)

#define ENGINE_DATA_ALIVE_POS   (5U)
#define ENGINE_DATA_CRC_POS     (7U)
#define BODY_STATUS_ALIVE_POS   (2U)
#define BODY_STATUS_CRC_POS     (3U)
#define CLUSTER_STATUS_ALIVE_POS (6U)
#define CLUSTER_STATUS_CRC_POS  (7U)

/* ---- Signal scaling (raw -> physical) ---------------------------------- */
#define ENGINE_RPM_RAW_PER_RPM  (4U)    /* 0.25 rpm/bit                    */
#define SPEED_RAW_PER_KMH       (100U)  /* 0.01 km/h/bit                   */
#define COOLANT_OFFSET_C        (40)    /* phys = raw - 40 degC            */
#define ODO_RAW_PER_KM          (10U)   /* 0.1 km/bit, 24 bit              */
#define ODO_RAW_MAX             (0xFFFFFFUL)

/* ClusterStatus.WarningBits */
#define WARN_OVERSPEED          (1U << 0)
#define WARN_COOLANT_HIGH       (1U << 1)
#define WARN_LOW_FUEL           (1U << 2)
#define WARN_ENGINE_LOST        (1U << 3)
#define WARN_BODY_LOST          (1U << 4)
#define WARN_SPEED_DEGRADED     (1U << 5)   /* showing wheel-pulse speed  */

/* ---- Message structures (raw values) ----------------------------------- */
typedef struct
{
    uint16_t engine_speed_raw;      /* 0.25 rpm/bit, 0..16383.75 rpm      */
    uint16_t vehicle_speed_raw;     /* 0.01 km/h/bit, 0..655.35 km/h      */
    uint8_t  coolant_temp_raw;      /* degC + 40                          */
    uint8_t  alive;                 /* 4 bit                              */
    uint8_t  crc;
} msg_engine_data_t;

typedef struct
{
    bool    turn_left;
    bool    turn_right;
    bool    high_beam;
    bool    door_open;
    bool    seatbelt_unfastened;
    uint8_t alive;
    uint8_t crc;
} msg_body_status_t;

typedef struct
{
    uint8_t  fuel_level_pct;        /* 0..100 %                           */
    uint32_t odometer_raw;          /* 0.1 km/bit, 24 bit                 */
    uint8_t  warning_bits;
    uint8_t  dtc_count;
    uint8_t  alive;
    uint8_t  crc;
} msg_cluster_status_t;

/* ---- Generic Intel bit access (start bit = LSB position, DBC "@1") ----- */
void     can_put_bits_le(uint8_t *buf, uint8_t start_bit, uint8_t len, uint32_t value);
uint32_t can_get_bits_le(const uint8_t *buf, uint8_t start_bit, uint8_t len);

/* ---- Pack / unpack (payload only, E2E applied separately) -------------- */
void can_pack_engine_data(const msg_engine_data_t *m, uint8_t buf[DLC_ENGINE_DATA]);
void can_unpack_engine_data(const uint8_t buf[DLC_ENGINE_DATA], msg_engine_data_t *m);

void can_pack_body_status(const msg_body_status_t *m, uint8_t buf[DLC_BODY_STATUS]);
void can_unpack_body_status(const uint8_t buf[DLC_BODY_STATUS], msg_body_status_t *m);

void can_pack_cluster_status(const msg_cluster_status_t *m, uint8_t buf[DLC_CLUSTER_STATUS]);
void can_unpack_cluster_status(const uint8_t buf[DLC_CLUSTER_STATUS], msg_cluster_status_t *m);

#endif /* CAN_MATRIX_H */
