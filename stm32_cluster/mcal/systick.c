/**
 * @file    systick.c
 * @brief   1 ms SysTick time base (see systick.h).
 */
#include "systick.h"
#include "board.h"

static volatile uint32_t s_tick_ms = 0UL;

void systick_init(uint32_t hclk_hz)
{
    SysTick->LOAD = (hclk_hz / 1000UL) - 1UL;      /* 24-bit reload: 71999 at 72 MHz */
    SysTick->VAL  = 0UL;
    NVIC_SetPriority(SysTick_IRQn, IRQ_PRIO_SYSTICK);
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk     /* HCLK, not HCLK/8 */
                  | SysTick_CTRL_TICKINT_Msk
                  | SysTick_CTRL_ENABLE_Msk;
}

void SysTick_Handler(void)
{
    s_tick_ms++;
}

uint32_t tick_get(void)
{
    return s_tick_ms;           /* aligned 32-bit load is single-copy atomic on Cortex-M3 */
}

bool tick_elapsed(uint32_t start_ms, uint32_t period_ms)
{
    return (uint32_t)(tick_get() - start_ms) >= period_ms;   /* correct across wrap */
}

void tick_delay_ms(uint32_t ms)
{
    const uint32_t start = tick_get();
    while (!tick_elapsed(start, ms))
    {
        __WFI();
    }
}
