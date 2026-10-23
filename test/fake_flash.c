/**
 * @file    fake_flash.c
 * @brief   RAM model of the NVM Flash page with real NOR semantics (program can only clear bits).
 */
#include "flash.h"
#include "fake_flash.h"
#include <string.h>

uint8_t  g_fake_flash[FLASH_PAGE_SIZE];
uint32_t g_fake_erase_count;
int32_t  g_fake_fail_after = -1;    /* simulate reset after N programmed half-words */

void fake_flash_reset(void)
{
    (void)memset(g_fake_flash, 0xFF, sizeof(g_fake_flash));
    g_fake_erase_count = 0U;
    g_fake_fail_after  = -1;
}

std_return_t flash_erase_page(uint32_t page_addr)
{
    if (page_addr != FLASH_NVM_PAGE_ADDR) { return E_NOT_OK; }
    (void)memset(g_fake_flash, 0xFF, sizeof(g_fake_flash));
    g_fake_erase_count++;
    return E_OK;
}

std_return_t flash_program(uint32_t addr, const uint8_t *data, uint16_t len)
{
    const uint32_t off = addr - FLASH_NVM_PAGE_ADDR;
    if (((len & 1U) != 0U) || ((addr & 1U) != 0U) || ((off + len) > FLASH_PAGE_SIZE)) { return E_NOT_OK; }
    for (uint16_t i = 0U; i < len; i = (uint16_t)(i + 2U))
    {
        if (g_fake_fail_after == 0) { return E_NOT_OK; }      /* "power lost" */
        if (g_fake_fail_after > 0) { g_fake_fail_after--; }
        /* STM32F1 refuses to program a half-word that is not erased (PGERR) */
        if ((g_fake_flash[off + i] != 0xFFU) || (g_fake_flash[off + i + 1U] != 0xFFU)) { return E_NOT_OK; }
        g_fake_flash[off + i]      = data[i];
        g_fake_flash[off + i + 1U] = data[i + 1U];
    }
    return E_OK;
}

const uint8_t *flash_ptr(uint32_t addr)
{
    return &g_fake_flash[addr - FLASH_NVM_PAGE_ADDR];
}
