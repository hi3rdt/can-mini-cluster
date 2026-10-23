/**
 * @file    flash.h
 * @brief   Internal Flash program/erase driver (STM32F103 medium density, 1 KB pages).
 */
#ifndef FLASH_H
#define FLASH_H

#include "std_types.h"

#define FLASH_PAGE_SIZE     (1024U)
#define FLASH_NVM_PAGE_ADDR (0x0800FC00UL)   /* last page of 64 KB, reserved in linker script */

std_return_t flash_erase_page(uint32_t page_addr);
std_return_t flash_program(uint32_t addr, const uint8_t *data, uint16_t len); /* len even */
const uint8_t *flash_ptr(uint32_t addr);

#endif /* FLASH_H */
