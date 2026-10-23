/**
 * @file    dashboard.h
 * @brief   320x240 instrument cluster screen with dirty-checked partial redraw.
 */
#ifndef DASHBOARD_H
#define DASHBOARD_H

#include "std_types.h"
#include "cluster.h"

void dashboard_init(void);
void dashboard_update(const cluster_state_t *st, uint32_t now_ms);

#endif /* DASHBOARD_H */
