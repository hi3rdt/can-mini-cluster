/**
 * @file    cli.h
 * @brief   Line-based debug console on USART1 (115200 8N1). Type "help".
 */
#ifndef CLI_H
#define CLI_H

#include "std_types.h"

void cli_init(void);
void cli_poll(uint32_t now_ms);     /* non-blocking, call from the 1 ms task */

#endif /* CLI_H */
