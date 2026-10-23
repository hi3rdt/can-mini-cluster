/**
 * @file    sched.c
 * @brief   Time-triggered cooperative scheduler (see sched.h).
 *
 * Every task runs to completion; ISRs only move data into queues, so the worst-case
 * latency of a task is the sum of the WCETs of the tasks released in the same tick.
 * When a task is released late by more than one period, it is counted as an overrun
 * and re-synchronised instead of being executed several times in a burst.
 */
#include "sched.h"
#include "systick.h"
#include "board.h"

#define LOAD_WINDOW_MS  (1000UL)

static sched_task_t *s_tasks;
static uint8_t       s_count;
static uint32_t      s_busy_cycles;
static uint32_t      s_window_start_ms;
static uint8_t       s_load_pct;
static uint8_t       s_load_peak_pct;

static bool reached(uint32_t now, uint32_t deadline)
{
    return (int32_t)(now - deadline) >= 0;
}

void sched_init(sched_task_t *tasks, uint8_t count)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;    /* enable DWT/ITM */
    DWT->CYCCNT = 0UL;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;

    s_tasks = tasks;
    s_count = count;
    const uint32_t now = tick_get();
    for (uint8_t i = 0U; i < count; i++)
    {
        tasks[i].next_ms = now + tasks[i].offset_ms;
    }
    s_window_start_ms = now;
}

void sched_run(void)
{
    for (;;)
    {
        const uint32_t now = tick_get();

        for (uint8_t i = 0U; i < s_count; i++)
        {
            sched_task_t *t = &s_tasks[i];
            if (!reached(now, t->next_ms))
            {
                continue;
            }

            const uint32_t c0 = DWT->CYCCNT;
            t->run(now);
            const uint32_t dt = DWT->CYCCNT - c0;       /* wrap-safe, 59 s range at 72 MHz */

            t->last_cycles = dt;
            if (dt > t->wcet_cycles)
            {
                t->wcet_cycles = dt;
            }
            t->runs++;
            s_busy_cycles += dt;

            t->next_ms += t->period_ms;
            if (reached(now, t->next_ms))
            {
                t->overruns++;
                t->next_ms = now + t->period_ms;
            }
        }

        if (reached(now, s_window_start_ms + LOAD_WINDOW_MS))
        {
            /* busy / (72e6 cycles per second) in percent */
            const uint32_t pct = s_busy_cycles / (SYSCLK_HZ / 100UL);
            s_load_pct = (uint8_t)((pct > 100UL) ? 100UL : pct);
            if (s_load_pct > s_load_peak_pct)
            {
                s_load_peak_pct = s_load_pct;
            }
            s_busy_cycles     = 0UL;
            s_window_start_ms = now;
        }

        __WFI();    /* sleep until SysTick or any IRQ: idle power and a clean load figure */
    }
}

uint8_t sched_cpu_load_pct(void)      { return s_load_pct; }
uint8_t sched_cpu_load_peak_pct(void) { return s_load_peak_pct; }
uint8_t sched_task_count(void)        { return s_count; }

const sched_task_t *sched_task(uint8_t idx)
{
    return (idx < s_count) ? &s_tasks[idx] : NULL;
}

uint32_t sched_cycles_to_us(uint32_t cycles)
{
    return cycles / (SYSCLK_HZ / 1000000UL);
}

void sched_reset_stats(void)
{
    for (uint8_t i = 0U; i < s_count; i++)
    {
        s_tasks[i].wcet_cycles = 0UL;
        s_tasks[i].overruns    = 0UL;
    }
    s_load_peak_pct = 0U;
}
