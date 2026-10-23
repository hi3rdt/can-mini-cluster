/**
 * @file    iwdg.c
 * @brief   Independent watchdog (see iwdg.h).
 */
#include "iwdg.h"
#include "stm32f1xx.h"

#define IWDG_KEY_UNLOCK     (0x5555U)
#define IWDG_KEY_REFRESH    (0xAAAAU)
#define IWDG_KEY_START      (0xCCCCU)
#define IWDG_PR_DIV32       (3U)
#define IWDG_TICK_HZ        (40000UL / 32UL)    /* 1.25 kHz nominal */
#define IWDG_RLR_MAX        (0x0FFFUL)

void iwdg_start(uint32_t timeout_ms)
{
    uint32_t rlr = ((timeout_ms * IWDG_TICK_HZ) / 1000UL);
    rlr = (rlr == 0UL) ? 1UL : ((rlr > (IWDG_RLR_MAX + 1UL)) ? (IWDG_RLR_MAX + 1UL) : rlr);

    DBGMCU->CR |= DBGMCU_CR_DBG_IWDG_STOP;      /* freeze while halted in the debugger */

    IWDG->KR  = IWDG_KEY_UNLOCK;
    IWDG->PR  = IWDG_PR_DIV32;
    IWDG->RLR = rlr - 1UL;
    while (IWDG->SR != 0UL) { }                 /* PVU/RVU: values transferred to LSI domain */
    IWDG->KR  = IWDG_KEY_REFRESH;
    IWDG->KR  = IWDG_KEY_START;
}

void iwdg_refresh(void)
{
    IWDG->KR = IWDG_KEY_REFRESH;
}
