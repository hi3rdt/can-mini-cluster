/**
 * @file    main.c
 * @brief   ESP32 vehicle simulator + UDS tester for the STM32 CAN mini cluster.
 *
 * Tasks:  can_rx (12)  RX dispatch, candump log      sim (10)     EngineData/BodyStatus
 *         can_alert(11) bus-off recovery               tester (8)   ISO-TP/UDS client
 *         console (5)  UART0 commands
 */
#include "can_if.h"
#include "console.h"
#include "sim.h"
#include "tester.h"

void app_main(void)
{
    tester_init();      /* first: its RX queue must exist before CAN frames are dispatched */
    can_if_init();
    sim_init();
    console_init();
}
