/**
 * @file    app_config.h
 * @brief   ESP32 vehicle simulator pin map and tuning.
 *
 * ESP32 GPIOs are NOT 5 V tolerant: with a TJA1050/MCP2551 (5 V) transceiver put a
 * 1k/2k divider on RXD, or use an SN65HVD230 (3.3 V) transceiver.
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#define PIN_TWAI_TX             (5)
#define PIN_TWAI_RX             (4)
#define PIN_SPEED_PULSE         (18)    /* -> STM32 PA1 (TIM2_CH2), 3.3 V logic */

#define SPEED_PULSE_HZ_PER_KMH  (2U)    /* must match the cluster's board.h */

#define TASK_PRIO_CAN_RX        (12)
#define TASK_PRIO_SIM           (10)
#define TASK_PRIO_TESTER        (8)
#define TASK_PRIO_CONSOLE       (5)

#endif /* APP_CONFIG_H */
