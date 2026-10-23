/**
 * @file    main.c
 * @brief   ESP32 vehicle simulator for the STM32 CAN mini cluster.
 *
 * FreeRTOS tasks (priority):  can_rx (12)    RX dispatch, ClusterStatus check, candump log
 *                             can_alert (11) error-passive / bus-off handling
 *                             sim (10)       EngineData 20 ms, BodyStatus 100 ms
 *                             console (5)    UART0 commands
 */
#include "can_if.h"
#include "console.h"
#include "sim.h"

void app_main(void)
{
    can_if_init();
    sim_init();
    console_init();
}
