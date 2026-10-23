#ifndef FAKE_FLASH_H
#define FAKE_FLASH_H

#include <stdint.h>

extern uint8_t  g_fake_flash[];
extern uint32_t g_fake_erase_count;
extern int32_t  g_fake_fail_after;

void fake_flash_reset(void);

#endif
