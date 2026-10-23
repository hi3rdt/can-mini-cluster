/**
 * @file    std_types.h
 * @brief   Basic types shared by every module (AUTOSAR-like Std_Types subset).
 */
#ifndef STD_TYPES_H
#define STD_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum
{
    E_OK      = 0,
    E_NOT_OK  = 1,
    E_TIMEOUT = 2,
    E_BUSY    = 3
} std_return_t;

#define ARRAY_LEN(a)    (sizeof(a) / sizeof((a)[0]))

#endif /* STD_TYPES_H */
