/**
 * @file    board.h
 * @brief   Blue Pill (STM32F103C8T6) pin map and system constants for the cluster ECU.
 *
 *  Function            Pin     Peripheral
 *  CAN RX / TX         PB8/PB9 bxCAN remap 2
 *  TFT SCK/MISO/MOSI   PA5/6/7 SPI1
 *  TFT CS / DC / RST   PA4 / PB0 / PB1
 *  TFT backlight       PA8     TIM1_CH1 PWM
 *  Debug UART TX / RX  PA9/PA10 USART1 115200 8N1
 *  Fuel level pot      PA0     ADC1_IN0
 *  Wheel speed pulse   PA1     TIM2_CH2 input capture
 *  Buzzer              PB6     TIM4_CH1 PWM
 *  Button MENU / TRIP  PB12/PB13 EXTI15_10, active low
 *  Status LED          PC13    active low
 */
#ifndef BOARD_H
#define BOARD_H

#include "stm32f1xx.h"

#define SYSCLK_HZ           (72000000UL)
#define HCLK_HZ             (SYSCLK_HZ)
#define PCLK1_HZ            (36000000UL)
#define PCLK2_HZ            (72000000UL)
#define TIMCLK_APB1_HZ      (72000000UL)    /* APB1 prescaler != 1 -> timer clock x2 */

#define UART_BAUD           (115200UL)

/* TFT control pins */
#define TFT_CS_PORT         GPIOA
#define TFT_CS_PIN          (4U)
#define TFT_DC_PORT         GPIOB
#define TFT_DC_PIN          (0U)
#define TFT_RST_PORT        GPIOB
#define TFT_RST_PIN         (1U)

/* Buttons */
#define BTN_PORT            GPIOB
#define BTN_MENU_PIN        (12U)
#define BTN_TRIP_PIN        (13U)

/* Status LED */
#define LED_PORT            GPIOC
#define LED_PIN             (13U)

/* Interrupt priorities (4 bits, 0 = highest, grouping: 4 bits pre-emption) */
#define IRQ_PRIO_TIM2       (3U)    /* speed capture timestamps are time-critical */
#define IRQ_PRIO_CAN_RX     (5U)
#define IRQ_PRIO_CAN_SCE    (6U)
#define IRQ_PRIO_USART1     (7U)
#define IRQ_PRIO_EXTI       (8U)
#define IRQ_PRIO_SYSTICK    (10U)

/* Application constants */
#define SPEED_PULSE_HZ_PER_KMH  (2U)     /* simulated wheel sensor: 2 Hz per km/h */
#define FW_VERSION_STR          "1.0.0"

#endif /* BOARD_H */
