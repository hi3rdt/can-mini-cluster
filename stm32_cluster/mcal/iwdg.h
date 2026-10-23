/**
 * @file    iwdg.h
 * @brief   Independent watchdog (LSI ~40 kHz, 30..60 kHz over temperature and parts).
 */
#ifndef IWDG_H
#define IWDG_H

#include "std_types.h"

/** Start with ~timeout_ms nominal (LSI 40 kHz, prescaler /32). Cannot be stopped. */
void iwdg_start(uint32_t timeout_ms);
void iwdg_refresh(void);

#endif /* IWDG_H */
