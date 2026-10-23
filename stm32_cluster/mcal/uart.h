/**
 * @file    uart.h
 * @brief   USART1 driver: interrupt-driven TX/RX ring buffers (non-blocking for the scheduler).
 */
#ifndef UART_H
#define UART_H

#include "std_types.h"

#define UART_TX_BUF_SIZE    (1024U)     /* power of two */
#define UART_RX_BUF_SIZE    (128U)      /* power of two */

void     uart_init(uint32_t pclk_hz, uint32_t baud);
/** Queue bytes for transmission; returns bytes accepted (rest dropped and counted). */
uint16_t uart_write(const uint8_t *data, uint16_t len);
/** Pop one received byte, false if none. */
bool     uart_getc(uint8_t *c);
/** Blocking polled output for fault handlers (interrupts may be dead). */
void     uart_write_polled(const char *s);
uint32_t uart_tx_dropped(void);
uint32_t uart_rx_overruns(void);

#endif /* UART_H */
