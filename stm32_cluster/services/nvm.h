/**
 * @file    nvm.h
 * @brief   Tiny append-only record store in one 1 KB Flash page.
 *
 * The page is split into 64-byte slots. Each write goes to the next erased slot;
 * the header magic is programmed last so a reset mid-write leaves an invalid slot
 * that is skipped on read. When the page is full it is erased and slot 0 reused.
 * (A two-page ping-pong scheme would survive power loss during erase: see docs.)
 */
#ifndef NVM_H
#define NVM_H

#include "std_types.h"

#define NVM_SLOT_SIZE       (64U)
#define NVM_HEADER_SIZE     (8U)
#define NVM_MAX_PAYLOAD     (NVM_SLOT_SIZE - NVM_HEADER_SIZE)

std_return_t nvm_init(void);
std_return_t nvm_read(void *dst, uint16_t len);         /* newest valid record */
std_return_t nvm_write(const void *src, uint16_t len);
uint16_t     nvm_write_count(void);                     /* sequence of newest record */

#endif /* NVM_H */
