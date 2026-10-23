/**
 * @file    dashboard.h
 * @brief   320x240 instrument cluster screen with dirty-checked partial redraw.
 */
#ifndef DASHBOARD_H
#define DASHBOARD_H

#include "std_types.h"
#include "cluster.h"

typedef enum
{
    INFO_ODO = 0,
    INFO_TRIP,
    INFO_CAN,
    INFO_SYSTEM,
    INFO_COUNT
} info_page_t;

void dashboard_init(void);
void dashboard_update(const cluster_state_t *st, uint32_t now_ms);
void dashboard_next_info_page(void);

#endif /* DASHBOARD_H */
