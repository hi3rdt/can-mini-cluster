/**
 * @file    buttons.c
 * @brief   Debounced buttons (see buttons.h).
 *
 * The EXTI ISR only accepts a falling edge if the previous accepted edge is older
 * than the debounce time; contact bounce (typ. 1..5 ms) is thus filtered without
 * timers or blocking. The press is latched as a flag that main consumes.
 */
#include "buttons.h"
#include "exti.h"
#include "gpio.h"
#include "systick.h"
#include "board.h"

#define DEBOUNCE_MS     (30UL)
#define PORT_INDEX_B    (1U)

static volatile uint32_t s_last_ms[BTN_COUNT];
static volatile bool     s_pressed[BTN_COUNT];

static void on_edge(uint8_t line)
{
    const button_t b = (line == BTN_MENU_PIN) ? BTN_MENU : BTN_TRIP;
    const uint32_t now = tick_get();

    if ((uint32_t)(now - s_last_ms[b]) >= DEBOUNCE_MS)
    {
        s_last_ms[b] = now;
        s_pressed[b] = true;
    }
}

void buttons_init(void)
{
    gpio_config_pullup(BTN_PORT, BTN_MENU_PIN);
    gpio_config_pullup(BTN_PORT, BTN_TRIP_PIN);
    (void)exti_config(PORT_INDEX_B, BTN_MENU_PIN, EXTI_EDGE_FALLING, on_edge);
    (void)exti_config(PORT_INDEX_B, BTN_TRIP_PIN, EXTI_EDGE_FALLING, on_edge);
}

bool buttons_take_press(button_t b)
{
    bool p = false;
    if (b < BTN_COUNT)
    {
        __disable_irq();
        p = s_pressed[b];
        s_pressed[b] = false;
        __enable_irq();
    }
    return p;
}
