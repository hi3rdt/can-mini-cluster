/**
 * @file    buttons.h
 * @brief   Two active-low push buttons on EXTI, debounced by timestamp (no blocking delay).
 */
#ifndef BUTTONS_H
#define BUTTONS_H

#include "std_types.h"

typedef enum
{
    BTN_MENU = 0,       /* cycles the info line on the dashboard */
    BTN_TRIP,           /* resets the trip counter */
    BTN_COUNT
} button_t;

void buttons_init(void);
/** True once per debounced press (main context). */
bool buttons_take_press(button_t b);

#endif /* BUTTONS_H */
