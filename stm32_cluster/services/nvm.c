/**
 * @file    nvm.c
 * @brief   Append-only record store on top of the Flash driver (see nvm.h).
 *
 * Slot layout (little-endian half-words):
 *   [0] magic  [1] sequence  [2] payload length  [3] CRC16 over payload
 *   [8..63] payload
 */
#include "nvm.h"
#include "flash.h"
#include <string.h>

#define NVM_MAGIC       (0xC1A5U)
#define NVM_ERASED      (0xFFFFU)
#define NVM_NUM_SLOTS   (FLASH_PAGE_SIZE / NVM_SLOT_SIZE)

static int16_t  s_newest = -1;      /* slot index of newest valid record, -1 = none */
static uint16_t s_seq    = 0U;

/* CRC-16/CCITT-FALSE, bitwise (called rarely) */
static uint16_t crc16(const uint8_t *d, uint16_t len)
{
    uint16_t crc = 0xFFFFU;
    for (uint16_t i = 0U; i < len; i++)
    {
        crc ^= (uint16_t)((uint16_t)d[i] << 8);
        for (uint8_t b = 0U; b < 8U; b++)
        {
            crc = ((crc & 0x8000U) != 0U) ? (uint16_t)((uint16_t)(crc << 1) ^ 0x1021U)
                                          : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t slot_addr(uint16_t slot)
{
    return FLASH_NVM_PAGE_ADDR + ((uint32_t)slot * NVM_SLOT_SIZE);
}

static bool slot_valid(uint16_t slot)
{
    const uint8_t *p = flash_ptr(slot_addr(slot));
    const uint16_t len = rd16(&p[4]);
    return (rd16(&p[0]) == NVM_MAGIC) && (len <= NVM_MAX_PAYLOAD) &&
           (crc16(&p[NVM_HEADER_SIZE], len) == rd16(&p[6]));
}

static bool slot_erased(uint16_t slot)
{
    const uint8_t *p = flash_ptr(slot_addr(slot));
    for (uint16_t i = 0U; i < NVM_SLOT_SIZE; i++)
    {
        if (p[i] != 0xFFU)
        {
            return false;
        }
    }
    return true;
}

std_return_t nvm_init(void)
{
    s_newest = -1;
    s_seq    = 0U;
    for (uint16_t slot = 0U; slot < NVM_NUM_SLOTS; slot++)
    {
        if (slot_valid(slot))
        {
            const uint16_t seq = rd16(&flash_ptr(slot_addr(slot))[2]);
            /* wrap-safe "newer than": sequence numbers are 16-bit */
            if ((s_newest < 0) || ((int16_t)(seq - s_seq) > 0))
            {
                s_newest = (int16_t)slot;
                s_seq    = seq;
            }
        }
    }
    return E_OK;
}

std_return_t nvm_read(void *dst, uint16_t len)
{
    if (s_newest < 0)
    {
        return E_NOT_OK;
    }
    const uint8_t *p = flash_ptr(slot_addr((uint16_t)s_newest));
    if (rd16(&p[4]) != len)
    {
        return E_NOT_OK;      /* layout changed (new firmware): treat as empty */
    }
    (void)memcpy(dst, &p[NVM_HEADER_SIZE], len);
    return E_OK;
}

std_return_t nvm_write(const void *src, uint16_t len)
{
    uint8_t slot_img[NVM_SLOT_SIZE];
    int16_t target = -1;

    if (len > NVM_MAX_PAYLOAD)
    {
        return E_NOT_OK;
    }

    /* Slots are filled in order, so the first erased slot after the newest is free */
    for (uint16_t slot = (uint16_t)(s_newest + 1); slot < NVM_NUM_SLOTS; slot++)
    {
        if (slot_erased(slot))
        {
            target = (int16_t)slot;
            break;
        }
    }
    if (target < 0)
    {
        if (flash_erase_page(FLASH_NVM_PAGE_ADDR) != E_OK)
        {
            return E_NOT_OK;
        }
        target = 0;
    }

    const uint16_t seq = (uint16_t)(s_seq + 1U);
    const uint16_t crc = crc16((const uint8_t *)src, len);

    (void)memset(slot_img, 0xFF, sizeof(slot_img));
    slot_img[2] = (uint8_t)seq; slot_img[3] = (uint8_t)(seq >> 8);
    slot_img[4] = (uint8_t)len; slot_img[5] = (uint8_t)(len >> 8);
    slot_img[6] = (uint8_t)crc; slot_img[7] = (uint8_t)(crc >> 8);
    (void)memcpy(&slot_img[NVM_HEADER_SIZE], src, len);

    const uint32_t base = slot_addr((uint16_t)target);
    const uint16_t prog_len = (uint16_t)((NVM_HEADER_SIZE + len + 1U) & ~1U);

    /* Body first, magic last: an interrupted write never looks valid */
    if (flash_program(base + 2U, &slot_img[2], (uint16_t)(prog_len - 2U)) != E_OK)
    {
        return E_NOT_OK;
    }
    const uint8_t magic[2] = { (uint8_t)NVM_MAGIC, (uint8_t)(NVM_MAGIC >> 8) };
    if (flash_program(base, magic, 2U) != E_OK)
    {
        return E_NOT_OK;
    }

    s_newest = target;
    s_seq    = seq;
    return slot_valid((uint16_t)target) ? E_OK : E_NOT_OK;   /* read-back verify */
}

uint16_t nvm_write_count(void)
{
    return s_seq;
}
