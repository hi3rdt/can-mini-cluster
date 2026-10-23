/**
 * @file    board.h
 * @brief   Blue Pill (STM32F103C8T6) pin map and system constants for the cluster ECU.
 *
 *  Function            Pin      Peripheral
 *  CAN RX / TX         PB8/PB9  bxCAN (remap 2) -> CAN transceiver
 *  TFT SCK/MISO/MOSI   PA5/6/7  SPI1
 *  TFT CS / DC / RST   PA4 / PB0 / PB1
 *  TFT backlight       PA8      GPIO, on
 *  Debug UART TX / RX  PA9/PA10 USART1 115200 8N1
 *  Fuel level pot      PA0      ADC1_IN0
 *  Status LED          PC13     active low, 1 Hz blink = main loop alive
 */
#ifndef BOARD_H
#define BOARD_H

#include "stm32f1xx.h"

#define SYSCLK_HZ           (72000000UL)
#define HCLK_HZ             (SYSCLK_HZ)
#define PCLK1_HZ            (36000000UL)
#define PCLK2_HZ            (72000000UL)

#define UART_BAUD           (115200UL)

/* TFT control pins */
#define TFT_CS_PORT         GPIOA
#define TFT_CS_PIN          (4U)
#define TFT_DC_PORT         GPIOB
#define TFT_DC_PIN          (0U)
#define TFT_RST_PORT        GPIOB
#define TFT_RST_PIN         (1U)
#define TFT_BL_PORT         GPIOA
#define TFT_BL_PIN          (8U)

/* Status LED */
#define LED_PORT            GPIOC
#define LED_PIN             (13U)

/* Interrupt priorities (4 bits, 0 = highest; grouping: 4 bits pre-emption) */
#define IRQ_PRIO_CAN_RX     (5U)    /* 3 HW FIFO slots at 500 kbit/s fill in ~0.7 ms */
#define IRQ_PRIO_CAN_SCE    (6U)
#define IRQ_PRIO_USART1     (7U)
#define IRQ_PRIO_SYSTICK    (10U)

#define WATCHDOG_TIMEOUT_MS (250UL)
#define FW_VERSION_STR      "1.0.0-core"

#endif /* BOARD_H */
