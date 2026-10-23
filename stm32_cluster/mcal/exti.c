/**
 * @file    exti.c
 * @brief   EXTI lines 10..15 (see exti.h).
 */
#include "exti.h"
#include "board.h"

#define EXTI_FIRST_LINE   (10U)
#define EXTI_LAST_LINE    (15U)

static exti_cb_t s_cb[EXTI_LAST_LINE - EXTI_FIRST_LINE + 1U];

std_return_t exti_config(uint8_t port_index, uint8_t line, exti_edge_t edge, exti_cb_t cb)
{
    if ((line < EXTI_FIRST_LINE) || (line > EXTI_LAST_LINE) || (port_index > 2U) || (cb == NULL))
    {
        return E_NOT_OK;
    }

    /* AFIO_EXTICR[line/4], 4 bits per line selects the port */
    const uint32_t shift = ((uint32_t)line & 3UL) * 4UL;
    AFIO->EXTICR[line >> 2] = (AFIO->EXTICR[line >> 2] & ~(0xFUL << shift)) |
                              ((uint32_t)port_index << shift);

    const uint32_t bit = 1UL << line;
    s_cb[line - EXTI_FIRST_LINE] = cb;
    if (((uint32_t)edge & (uint32_t)EXTI_EDGE_RISING) != 0UL)  { EXTI->RTSR |= bit; }
    if (((uint32_t)edge & (uint32_t)EXTI_EDGE_FALLING) != 0UL) { EXTI->FTSR |= bit; }
    EXTI->PR  = bit;          /* clear stale pending flag (rc_w1) */
    EXTI->IMR |= bit;

    NVIC_SetPriority(EXTI15_10_IRQn, IRQ_PRIO_EXTI);
    NVIC_EnableIRQ(EXTI15_10_IRQn);
    return E_OK;
}

void EXTI15_10_IRQHandler(void)
{
    const uint32_t pending = EXTI->PR & EXTI->IMR & 0xFC00UL;
    EXTI->PR = pending;       /* clear first: an edge during the callback is not lost */

    for (uint8_t line = EXTI_FIRST_LINE; line <= EXTI_LAST_LINE; line++)
    {
        if (((pending >> line) & 1UL) != 0UL)
        {
            s_cb[line - EXTI_FIRST_LINE](line);
        }
    }
}
