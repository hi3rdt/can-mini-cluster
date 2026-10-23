/**
 * @file    rcc.h
 * @brief   Clock tree setup (HSE 8 MHz -> PLL x9 = 72 MHz) and reset-cause readout.
 */
#ifndef RCC_H
#define RCC_H

#include "std_types.h"

typedef enum
{
    RESET_CAUSE_POWER_ON = 0,
    RESET_CAUSE_PIN,
    RESET_CAUSE_SOFTWARE,
    RESET_CAUSE_IWDG,
    RESET_CAUSE_WWDG,
    RESET_CAUSE_LOW_POWER,
    RESET_CAUSE_UNKNOWN
} reset_cause_t;

/** E_OK: running at 72 MHz from HSE. E_NOT_OK: HSE failed, still on HSI 8 MHz. */
std_return_t  rcc_init_72mhz(void);
/** Read RCC_CSR once at boot, then clear the flags (RMVF). */
reset_cause_t rcc_reset_cause(void);
const char   *rcc_reset_cause_str(reset_cause_t rc);

#endif /* RCC_H */
