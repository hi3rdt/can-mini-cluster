/**
 * @file    cluster.h
 * @brief   Cluster application logic: signal selection, plausibility, warnings, odometer.
 *
 * Produces one consistent cluster_state_t snapshot per 20 ms that the dashboard
 * renders and the ClusterStatus message reports - display and bus never disagree.
 */
#ifndef CLUSTER_H
#define CLUSTER_H

#include "std_types.h"

#define OVERSPEED_KMH           (120U)
#define COOLANT_WARN_C          (110)
#define LOW_FUEL_ON_PCT         (10U)
#define LOW_FUEL_OFF_PCT        (12U)   /* hysteresis */
#define PLAUSIBILITY_DIFF_KMH   (10U)
#define PLAUSIBILITY_TIME_MS    (2000UL)
#define BELT_CHIME_KMH          (20U)

typedef enum
{
    SPEED_SRC_NONE = 0,     /* no source: "---" */
    SPEED_SRC_CAN,          /* EngineData.VehicleSpeed */
    SPEED_SRC_WHEEL         /* degraded mode: local wheel pulse */
} speed_src_t;

typedef struct
{
    speed_src_t speed_src;
    uint16_t    speed_kmh;
    bool        rpm_valid;
    uint16_t    rpm;
    bool        coolant_valid;
    int16_t     coolant_c;
    uint8_t     fuel_pct;
    uint32_t    odometer_x10;   /* 0.1 km */
    uint32_t    trip_x10;
    bool        body_valid;
    bool        turn_left;
    bool        turn_right;
    bool        high_beam;
    bool        door_open;
    bool        belt_unfastened;
    uint8_t     warnings;       /* WARN_* from can_matrix.h */
    uint8_t     dtc_count;
} cluster_state_t;

void cluster_init(void);
void cluster_main_20ms(uint32_t now_ms);
void cluster_main_100ms(uint32_t now_ms);   /* ClusterStatus TX */
const cluster_state_t *cluster_state(void);
void cluster_reset_trip(void);

#endif /* CLUSTER_H */
