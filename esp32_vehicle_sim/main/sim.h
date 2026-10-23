/**
 * @file    sim.h
 * @brief   Vehicle model: EngineData (20 ms) / BodyStatus (100 ms) senders, wheel-speed
 *          pulse output and fault injection for HIL-style tests of the cluster.
 */
#ifndef SIM_H
#define SIM_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    SIM_MODE_SWEEP = 0,     /* automatic drive cycle for demos */
    SIM_MODE_MANUAL         /* values set from the console */
} sim_mode_t;

typedef struct
{
    sim_mode_t mode;
    float      speed_kmh;
    float      rpm;
    float      coolant_c;
    bool       turn_left, turn_right, high_beam, door_open, belt_unfastened;
    bool       pulse_enabled;
    bool       mute_engine, mute_body;  /* stop sending -> cluster timeout DTCs */
    bool       corrupt_crc;             /* send wrong CRC -> cluster E2E DTC    */
    float      pulse_offset_kmh;        /* wheel vs CAN mismatch -> plausibility DTC */
} sim_cfg_t;

void sim_init(void);
sim_cfg_t *sim_cfg(void);   /* console writes single fields; floats/bools are atomic on ESP32 */

#endif /* SIM_H */
