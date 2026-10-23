/**
 * @file    rcc.c
 * @brief   Clock tree and reset cause (see rcc.h).
 */
#include "rcc.h"
#include "stm32f1xx.h"

/* Loop-count timeouts: SysTick is not running yet. HSE start-up is ~2 ms worst case. */
#define HSE_TIMEOUT_LOOPS   (100000UL)
#define PLL_TIMEOUT_LOOPS   (100000UL)

static bool wait_flag(volatile const uint32_t *reg, uint32_t mask, uint32_t expect, uint32_t loops)
{
    while (((*reg & mask) != expect) && (loops > 0UL))
    {
        loops--;
    }
    return loops > 0UL;
}

std_return_t rcc_init_72mhz(void)
{
    RCC->CR |= RCC_CR_HSEON;
    if (!wait_flag(&RCC->CR, RCC_CR_HSERDY, RCC_CR_HSERDY, HSE_TIMEOUT_LOOPS))
    {
        RCC->CR &= ~RCC_CR_HSEON;
        return E_NOT_OK;            /* crystal missing/broken: stay on HSI */
    }

    /* 2 wait states for 48 < SYSCLK <= 72 MHz. Note: FLASH_ACR_LATENCY_2 in this CMSIS
       version is bit 2 (value 4), not "two wait states", hence the explicit shift. */
    FLASH->ACR = FLASH_ACR_PRFTBE | (2UL << FLASH_ACR_LATENCY_Pos);

    RCC->CFGR = RCC_CFGR_HPRE_DIV1      /* HCLK  = 72 MHz           */
              | RCC_CFGR_PPRE1_DIV2     /* PCLK1 = 36 MHz (max 36)  */
              | RCC_CFGR_PPRE2_DIV1     /* PCLK2 = 72 MHz           */
              | RCC_CFGR_ADCPRE_DIV6    /* ADCCLK = 12 MHz (max 14) */
              | RCC_CFGR_PLLSRC         /* PLL input = HSE          */
              | RCC_CFGR_PLLMULL9;

    RCC->CR |= RCC_CR_PLLON;
    if (!wait_flag(&RCC->CR, RCC_CR_PLLRDY, RCC_CR_PLLRDY, PLL_TIMEOUT_LOOPS))
    {
        return E_NOT_OK;
    }

    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    if (!wait_flag(&RCC->CFGR, RCC_CFGR_SWS, RCC_CFGR_SWS_PLL, PLL_TIMEOUT_LOOPS))
    {
        return E_NOT_OK;
    }
    return E_OK;
}

reset_cause_t rcc_reset_cause(void)
{
    const uint32_t csr = RCC->CSR;
    reset_cause_t rc;

    /* Priority matters: a watchdog reset also sets PINRSTF (NRST is driven low) */
    if ((csr & RCC_CSR_LPWRRSTF) != 0UL)      { rc = RESET_CAUSE_LOW_POWER; }
    else if ((csr & RCC_CSR_IWDGRSTF) != 0UL) { rc = RESET_CAUSE_IWDG; }
    else if ((csr & RCC_CSR_WWDGRSTF) != 0UL) { rc = RESET_CAUSE_WWDG; }
    else if ((csr & RCC_CSR_SFTRSTF) != 0UL)  { rc = RESET_CAUSE_SOFTWARE; }
    else if ((csr & RCC_CSR_PORRSTF) != 0UL)  { rc = RESET_CAUSE_POWER_ON; }
    else if ((csr & RCC_CSR_PINRSTF) != 0UL)  { rc = RESET_CAUSE_PIN; }
    else                                      { rc = RESET_CAUSE_UNKNOWN; }

    RCC->CSR |= RCC_CSR_RMVF;
    return rc;
}

const char *rcc_reset_cause_str(reset_cause_t rc)
{
    static const char *const names[] = {
        "power-on", "pin", "software", "iwdg", "wwdg", "low-power", "unknown"
    };
    return ((uint32_t)rc < ARRAY_LEN(names)) ? names[rc] : "unknown";
}
