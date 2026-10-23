/**
 * @file    flash.c
 * @brief   Internal Flash erase/program (see flash.h). RM0008 section 3.3 / PM0075.
 */
#include "flash.h"
#include "stm32f1xx.h"

/* FLASH_KEY1/FLASH_KEY2 unlock keys come from stm32f103xb.h */
#define FLASH_BUSY_LOOPS    (2000000UL)     /* > 40 ms page erase at 72 MHz */

static void flash_unlock(void)
{
    if ((FLASH->CR & FLASH_CR_LOCK) != 0UL)
    {
        FLASH->KEYR = FLASH_KEY1;
        FLASH->KEYR = FLASH_KEY2;
    }
}

static void flash_lock(void)
{
    FLASH->CR |= FLASH_CR_LOCK;
}

static std_return_t flash_wait(void)
{
    uint32_t loops = FLASH_BUSY_LOOPS;
    while (((FLASH->SR & FLASH_SR_BSY) != 0UL) && (loops > 0UL))
    {
        loops--;
    }
    const uint32_t sr = FLASH->SR;
    FLASH->SR = FLASH_SR_EOP | FLASH_SR_PGERR | FLASH_SR_WRPRTERR;   /* rc_w1 */
    if ((loops == 0UL) || ((sr & (FLASH_SR_PGERR | FLASH_SR_WRPRTERR)) != 0UL))
    {
        return E_NOT_OK;
    }
    return E_OK;
}

static bool in_nvm_page(uint32_t addr, uint32_t len)
{
    return (addr >= FLASH_NVM_PAGE_ADDR) && ((addr + len) <= (FLASH_NVM_PAGE_ADDR + FLASH_PAGE_SIZE));
}

std_return_t flash_erase_page(uint32_t page_addr)
{
    if (!in_nvm_page(page_addr, FLASH_PAGE_SIZE))
    {
        return E_NOT_OK;            /* never erase application code */
    }
    flash_unlock();
    FLASH->CR |= FLASH_CR_PER;
    FLASH->AR  = page_addr;
    FLASH->CR |= FLASH_CR_STRT;
    const std_return_t rc = flash_wait();
    FLASH->CR &= ~FLASH_CR_PER;
    flash_lock();
    return rc;
}

std_return_t flash_program(uint32_t addr, const uint8_t *data, uint16_t len)
{
    std_return_t rc = E_OK;

    if (((addr & 1UL) != 0UL) || ((len & 1U) != 0U) || !in_nvm_page(addr, len))
    {
        return E_NOT_OK;
    }
    flash_unlock();
    FLASH->CR |= FLASH_CR_PG;
    for (uint16_t i = 0U; (i < len) && (rc == E_OK); i = (uint16_t)(i + 2U))
    {
        /* F1 Flash is programmed by 16-bit writes only */
        *(volatile uint16_t *)(addr + i) = (uint16_t)((uint16_t)data[i] | ((uint16_t)data[i + 1U] << 8));
        rc = flash_wait();
    }
    FLASH->CR &= ~FLASH_CR_PG;
    flash_lock();
    return rc;
}

const uint8_t *flash_ptr(uint32_t addr)
{
    return (const uint8_t *)addr;
}
