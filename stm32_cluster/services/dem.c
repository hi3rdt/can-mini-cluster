/**
 * @file    dem.c
 * @brief   Diagnostic Event Manager (see dem.h).
 *
 * Simplifications vs. a production DEM: events are debounced by the caller
 * (timeouts are inherently debounced), a DTC is confirmed on its first failed
 * report, and there is no aging or freeze frame (occurrence counter only).
 */
#include "dem.h"
#include "nvm.h"
#include <string.h>

typedef struct
{
    uint8_t status[DEM_EVT_COUNT];
    uint8_t occurrences[DEM_EVT_COUNT];
} dem_nv_t;

static const uint32_t s_dtc_codes[DEM_EVT_COUNT] = {
    0xC10000UL,     /* U0100 */
    0xC14000UL,     /* U0140 */
    0xC00100UL,     /* U0001 */
    0xC40100UL,     /* U0401 */
    0x900000UL,     /* B1000 */
    0x900200UL      /* B1002 */
};

static dem_nv_t s_dem;
static bool     s_dirty;

/* Only CDTC, TFSLC and the counters survive a power cycle */
#define DEM_ST_PERSISTENT   (DEM_ST_CDTC | DEM_ST_TFSLC)

void dem_init(void)
{
    if (nvm_read(&s_dem, (uint16_t)sizeof(s_dem)) != E_OK)
    {
        (void)memset(&s_dem, 0, sizeof(s_dem));
    }
    for (uint8_t i = 0U; i < (uint8_t)DEM_EVT_COUNT; i++)
    {
        s_dem.status[i] &= DEM_ST_PERSISTENT;   /* new operation cycle */
    }
    s_dirty = false;
}

void dem_report(dem_event_t evt, bool failed)
{
    if (evt >= DEM_EVT_COUNT)
    {
        return;
    }
    uint8_t *st = &s_dem.status[evt];

    if (failed)
    {
        if ((*st & DEM_ST_TF) == 0U)            /* count passed -> failed edges */
        {
            if (s_dem.occurrences[evt] < 0xFFU)
            {
                s_dem.occurrences[evt]++;
            }
            s_dirty = true;
        }
        if ((*st & DEM_ST_CDTC) == 0U)
        {
            s_dirty = true;
        }
        *st |= (uint8_t)(DEM_ST_TF | DEM_ST_TFTOC | DEM_ST_PDTC | DEM_ST_CDTC | DEM_ST_TFSLC);
    }
    else
    {
        *st &= (uint8_t)~DEM_ST_TF;             /* CDTC stays until ClearDTC */
    }
}

void dem_clear_all(void)
{
    (void)memset(&s_dem, 0, sizeof(s_dem));
    s_dirty = true;
}

void dem_main_100ms(void)
{
    /* Single-bank F103: the CPU stalls during Flash ops (~50 us/half-word, 20-40 ms/page
     * erase), so writes are deferred here and never issued from an ISR. */
    if (s_dirty)
    {
        if (nvm_write(&s_dem, (uint16_t)sizeof(s_dem)) == E_OK)
        {
            s_dirty = false;
        }
    }
}

uint32_t dem_dtc_code(dem_event_t evt)
{
    return (evt < DEM_EVT_COUNT) ? s_dtc_codes[evt] : 0UL;
}

uint8_t dem_status(dem_event_t evt)
{
    return (evt < DEM_EVT_COUNT) ? s_dem.status[evt] : 0U;
}

uint8_t dem_occurrences(dem_event_t evt)
{
    return (evt < DEM_EVT_COUNT) ? s_dem.occurrences[evt] : 0U;
}

uint16_t dem_count_by_mask(uint8_t mask)
{
    uint16_t n = 0U;
    for (uint8_t i = 0U; i < (uint8_t)DEM_EVT_COUNT; i++)
    {
        if ((s_dem.status[i] & mask & DEM_STATUS_AVAILABILITY_MASK) != 0U)
        {
            n++;
        }
    }
    return n;
}

uint16_t dem_get_by_mask(uint8_t mask, uint8_t *out, uint16_t max)
{
    uint16_t pos = 0U;
    for (uint8_t i = 0U; i < (uint8_t)DEM_EVT_COUNT; i++)
    {
        if (((s_dem.status[i] & mask & DEM_STATUS_AVAILABILITY_MASK) != 0U) &&
            ((uint16_t)(pos + 4U) <= max))
        {
            out[pos]      = (uint8_t)(s_dtc_codes[i] >> 16);
            out[pos + 1U] = (uint8_t)(s_dtc_codes[i] >> 8);
            out[pos + 2U] = (uint8_t)(s_dtc_codes[i]);
            out[pos + 3U] = (uint8_t)(s_dem.status[i] & DEM_STATUS_AVAILABILITY_MASK);
            pos = (uint16_t)(pos + 4U);
        }
    }
    return pos;
}

uint8_t dem_confirmed_count(void)
{
    return (uint8_t)dem_count_by_mask(DEM_ST_CDTC);
}
