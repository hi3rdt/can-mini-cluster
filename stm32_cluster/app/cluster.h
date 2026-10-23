/**
 * @file    cluster.h
 * @brief   Cluster application logic: signal conversion, warnings, fuel filter, odometer.
 *
 * Produces one consistent cluster_state_t snapshot every 20 ms. The dashboard draws
 * it and the ClusterStatus message reports it, so display and bus never disagree.
 */
#ifndef CLUSTER_H
#define CLUSTER_H

#include "std_types.h"

#define OVERSPEED_KMH           (120U)
#define COOLANT_WARN_C          (110)
#define LOW_FUEL_ON_PCT         (10U)
#define LOW_FUEL_OFF_PCT        (12U)   /* hysteresis */

typedef struct
{
    bool        engine_valid;   /* EngineData received and not timed out */
    uint16_t    speed_kmh;
    uint16_t    rpm;
    int16_t     coolant_c;
    uint8_t     fuel_pct;       /* local sensor, always valid */
    uint32_t    odometer_x10;   /* 0.1 km, RAM only (reset at power-up) */
    bool        body_valid;
    bool        turn_left;
    bool        turn_right;
    bool        high_beam;
    bool        door_open;
    bool        belt_unfastened;
    uint8_t     warnings;       /* WARN_* from can_matrix.h */
} cluster_state_t;

void cluster_init(void);
void cluster_update_20ms(void);
void cluster_send_status_100ms(void);
const cluster_state_t *cluster_state(void);

#endif /* CLUSTER_H */
