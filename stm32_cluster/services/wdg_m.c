/**
 * @file    wdg_m.c
 * @brief   Watchdog manager (see wdg_m.h).
 *
 * Each supervised task sets its bit at least once per supervision window. The
 * IWDG is only refreshed if every expected bit is present, so a task that hangs,
 * starves, or is never scheduled again leads to a reset within ~250 ms - not only
 * a stuck main loop as with a naive "kick in the idle loop".
 */
#include "wdg_m.h"
#include "iwdg.h"

static uint32_t s_expected;
static uint32_t s_alive;            /* written by tasks, all in main context */
static bool     s_stall;

void wdg_m_init(uint32_t expected_mask)
{
    s_expected = expected_mask;
    s_alive    = 0UL;
    s_stall    = false;
    iwdg_start(WDG_M_TIMEOUT_MS);
}

void wdg_m_checkpoint(uint32_t task_bit)
{
    s_alive |= task_bit;
}

void wdg_m_main_100ms(void)
{
    if (((s_alive & s_expected) == s_expected) && !s_stall)
    {
        iwdg_refresh();
    }
    s_alive = 0UL;
}

void wdg_m_test_stall(void)
{
    s_stall = true;
}
