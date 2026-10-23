/**
 * @file    app_info.c
 * @brief   DID provider and runtime figures (see app_info.h).
 */
#include "app_info.h"
#include "cluster.h"
#include "sched.h"
#include "bxcan.h"
#include "adc.h"
#include "systick.h"
#include "board.h"
#include <string.h>

#define STACK_PAINT_PATTERN (0xA5A5A5A5UL)
#define UID_BASE_ADDR       (0x1FFFF7E8UL)  /* RM0008 30.2: 96-bit unique device ID */

extern uint32_t _ebss, _estack;

static const char    s_vin[] = "VNDEMOCLUSTER0001";
static reset_cause_t s_reset_cause;

static uint16_t put_be(uint8_t *out, uint16_t max, uint32_t v, uint8_t n)
{
    if (max < n)
    {
        return 0U;
    }
    for (uint8_t i = 0U; i < n; i++)
    {
        out[i] = (uint8_t)(v >> (8U * (uint8_t)(n - 1U - i)));
    }
    return n;
}

static uint16_t put_bytes(uint8_t *out, uint16_t max, const void *src, uint16_t n)
{
    if (max < n)
    {
        return 0U;
    }
    (void)memcpy(out, src, n);
    return n;
}

void app_info_init(reset_cause_t rc)
{
    s_reset_cause = rc;
}

uint16_t app_read_did(uint16_t did, uint8_t *out, uint16_t max)
{
    const cluster_state_t *st = cluster_state();
    uint16_t n = 0U;

    switch (did)
    {
    case DID_VIN:           n = put_bytes(out, max, s_vin, 17U);                                   break;
    case DID_SW_VERSION:    n = put_bytes(out, max, FW_VERSION_STR, (uint16_t)(sizeof(FW_VERSION_STR) - 1U)); break;
    case DID_ECU_SERIAL:    n = put_bytes(out, max, (const void *)UID_BASE_ADDR, 12U);             break;
    case DID_VEHICLE_SPEED: n = put_be(out, max, st->speed_kmh, 2U);                               break;
    case DID_FUEL_LEVEL:    n = put_be(out, max, st->fuel_pct, 1U);                                break;
    case DID_CPU_LOAD:
        n = put_be(out, max, ((uint32_t)sched_cpu_load_pct() << 8) | sched_cpu_load_peak_pct(), 2U);
        break;
    case DID_RESET_CAUSE:   n = put_be(out, max, (uint32_t)s_reset_cause, 1U);                    break;
    case DID_ODOMETER:      n = put_be(out, max, st->odometer_x10, 4U);                            break;
    case DID_MCU_TEMP:      n = put_be(out, max, (uint32_t)(uint16_t)adc_mcu_temp_x10(), 2U);      break;
    case DID_CAN_ERRORS:
    {
        can_stats_t cs;
        can_get_stats(&cs);
        if (max >= 6U)
        {
            out[0] = cs.tec;
            out[1] = cs.rec;
            n = (uint16_t)(2U + put_be(&out[2], (uint16_t)(max - 2U), cs.bus_off_events, 4U));
        }
        break;
    }
    case DID_UPTIME:        n = put_be(out, max, tick_get() / 1000UL, 4U);                         break;
    default:                n = 0U;                                                                 break;
    }
    return n;
}

uint32_t app_stack_used_bytes(void)
{
    /* Startup painted [_ebss, SP-64) with the pattern; the first overwritten word marks
       the deepest stack excursion so far (high-water mark). */
    const uint32_t *p = &_ebss;
    while ((p < &_estack) && (*p == STACK_PAINT_PATTERN))
    {
        p++;
    }
    return (uint32_t)&_estack - (uint32_t)p;
}

uint32_t app_stack_total_bytes(void)
{
    return (uint32_t)&_estack - (uint32_t)&_ebss;
}
