/**
 * @file    uart.c
 * @brief   USART1 on PA9 (TX) / PA10 (RX) with SPSC ring buffers (see uart.h).
 *
 * Concurrency: TX head is written by main only, TX tail by the ISR only (and the
 * reverse for RX), so no critical section is needed for the indices themselves.
 * Enabling TXEIE from main after updating the head is the only shared action and
 * is a single atomic register write.
 */
#include "uart.h"
#include "gpio.h"
#include "board.h"

#define TX_MASK     (UART_TX_BUF_SIZE - 1U)
#define RX_MASK     (UART_RX_BUF_SIZE - 1U)

static uint8_t           s_tx_buf[UART_TX_BUF_SIZE];
static volatile uint16_t s_tx_head;     /* written by main */
static volatile uint16_t s_tx_tail;     /* written by ISR  */
static uint8_t           s_rx_buf[UART_RX_BUF_SIZE];
static volatile uint16_t s_rx_head;     /* written by ISR  */
static volatile uint16_t s_rx_tail;     /* written by main */
static uint32_t          s_tx_dropped;
static volatile uint32_t s_rx_overruns;

void uart_init(uint32_t pclk_hz, uint32_t baud)
{
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    gpio_config(GPIOA, 9U, GPIO_AF_PP_50MHZ);
    gpio_config_pullup(GPIOA, 10U);       /* idle-high even when RX is unconnected */

    USART1->BRR = (pclk_hz + (baud / 2UL)) / baud;   /* 72 MHz / 115200 -> 625 (0.0 % error) */
    USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_UE;

    NVIC_SetPriority(USART1_IRQn, IRQ_PRIO_USART1);
    NVIC_EnableIRQ(USART1_IRQn);
}

uint16_t uart_write(const uint8_t *data, uint16_t len)
{
    uint16_t n = 0U;
    uint16_t head = s_tx_head;

    while (n < len)
    {
        const uint16_t next = (uint16_t)((head + 1U) & TX_MASK);
        if (next == s_tx_tail)
        {
            s_tx_dropped += (uint32_t)(len - n);   /* full: never block the scheduler */
            break;
        }
        s_tx_buf[head] = data[n];
        head = next;
        n++;
    }
    s_tx_head = head;
    USART1->CR1 |= USART_CR1_TXEIE;   /* RMW on CR1: ISR only clears TXEIE, which is benign */
    return n;
}

bool uart_getc(uint8_t *c)
{
    const uint16_t tail = s_rx_tail;
    if (tail == s_rx_head)
    {
        return false;
    }
    *c = s_rx_buf[tail];
    s_rx_tail = (uint16_t)((tail + 1U) & RX_MASK);
    return true;
}

void uart_write_polled(const char *s)
{
    USART1->CR1 &= ~USART_CR1_TXEIE;
    while (*s != '\0')
    {
        while ((USART1->SR & USART_SR_TXE) == 0UL) { }
        USART1->DR = (uint8_t)*s;
        s++;
    }
    while ((USART1->SR & USART_SR_TC) == 0UL) { }
}

uint32_t uart_tx_dropped(void)  { return s_tx_dropped; }
uint32_t uart_rx_overruns(void) { return s_rx_overruns; }

void USART1_IRQHandler(void)
{
    const uint32_t sr = USART1->SR;

    if ((sr & (USART_SR_RXNE | USART_SR_ORE)) != 0UL)
    {
        /* Reading DR clears RXNE and, after the SR read above, ORE as well */
        const uint8_t c = (uint8_t)USART1->DR;
        const uint16_t next = (uint16_t)((s_rx_head + 1U) & RX_MASK);
        if (((sr & USART_SR_ORE) != 0UL) || (next == s_rx_tail))
        {
            s_rx_overruns++;
        }
        if (next != s_rx_tail)
        {
            s_rx_buf[s_rx_head] = c;
            s_rx_head = next;
        }
    }

    if (((USART1->CR1 & USART_CR1_TXEIE) != 0UL) && ((sr & USART_SR_TXE) != 0UL))
    {
        const uint16_t tail = s_tx_tail;
        if (tail == s_tx_head)
        {
            USART1->CR1 &= ~USART_CR1_TXEIE;
        }
        else
        {
            USART1->DR = s_tx_buf[tail];
            s_tx_tail = (uint16_t)((tail + 1U) & TX_MASK);
        }
    }
}
