/**
 * @file    log.h
 * @brief   Tiny printf subset to the UART TX ring (no heap, no float, no newlib stdio).
 *
 * Supported: %d %u %x %X %s %c %% with optional '0' flag and width, and 'l' (ignored,
 * int and long are both 32 bit on Cortex-M3).
 * Uses stdarg.h; every call site is type-checked through the format attribute.
 */
#ifndef LOG_H
#define LOG_H

#include "std_types.h"

void     log_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
/** Format into buf (always NUL-terminated), returns length. */
uint16_t log_snprintf(char *buf, uint16_t size, const char *fmt, ...) __attribute__((format(printf, 3, 4)));

#endif /* LOG_H */
