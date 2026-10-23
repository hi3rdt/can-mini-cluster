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

#define TASK_PRIO_CAN_RX        (12)
#define TASK_PRIO_SIM           (10)
#define TASK_PRIO_CONSOLE       (5)

#endif /* APP_CONFIG_H */
