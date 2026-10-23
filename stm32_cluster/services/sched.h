/**
 * @file    sched.h
 * @brief   Cooperative time-triggered scheduler with DWT-based WCET and CPU-load measurement.
 */
#ifndef SCHED_H
#define SCHED_H

#include "std_types.h"

typedef struct
{
    const char *name;
    void      (*run)(uint32_t now_ms);
    uint32_t    period_ms;
    uint32_t    offset_ms;      /* phase shift so tasks do not all fire in the same tick */
    /* runtime data */
    uint32_t    next_ms;
    uint32_t    wcet_cycles;
    uint32_t    last_cycles;
    uint32_t    runs;
    uint32_t    overruns;
} sched_task_t;

void     sched_init(sched_task_t *tasks, uint8_t count);
void     sched_run(void);                       /* never returns */
uint8_t  sched_cpu_load_pct(void);              /* busy share of the last 1 s window */
uint8_t  sched_cpu_load_peak_pct(void);
const sched_task_t *sched_task(uint8_t idx);
uint8_t  sched_task_count(void);
uint32_t sched_cycles_to_us(uint32_t cycles);
void     sched_reset_stats(void);

#endif /* SCHED_H */
