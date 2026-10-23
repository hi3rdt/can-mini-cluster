/**
 * @file    wdg_m.h
 * @brief   Watchdog manager: alive supervision of scheduler tasks before refreshing the IWDG.
 */
#ifndef WDG_M_H
#define WDG_M_H

#include "std_types.h"

#define WDG_M_TIMEOUT_MS        (250UL)     /* nominal; LSI tolerance makes it 167..333 ms */
#define WDG_M_SUPERVISION_MS    (100UL)

void wdg_m_init(uint32_t expected_mask);
void wdg_m_checkpoint(uint32_t task_bit);
void wdg_m_main_100ms(void);
void wdg_m_test_stall(void);    /* CLI fault injection: stop checking in -> IWDG reset */

#endif /* WDG_M_H */
