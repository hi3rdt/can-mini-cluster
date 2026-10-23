/**
 * @file    dem.h
 * @brief   Diagnostic Event Manager: DTC status bytes (ISO 14229-1 Annex D) + NVM persistence.
 */
#ifndef DEM_H
#define DEM_H

#include "std_types.h"

typedef enum
{
    DEM_EVT_ENGINE_TIMEOUT = 0,     /* U0100 lost communication with ECM        */
    DEM_EVT_BODY_TIMEOUT,           /* U0140 lost communication with BCM        */
    DEM_EVT_CAN_BUSOFF,             /* U0001 high-speed CAN bus                 */
    DEM_EVT_ENGINE_E2E,             /* U0401 invalid data received from ECM     */
    DEM_EVT_WDG_RESET,              /* B1000 ECU internal: watchdog reset       */
    DEM_EVT_SPEED_PLAUSIBILITY,     /* B1002 CAN speed vs wheel pulse mismatch  */
    DEM_EVT_COUNT
} dem_event_t;

/* DTC status bits supported (statusAvailabilityMask) */
#define DEM_ST_TF       (0x01U)     /* testFailed                     */
#define DEM_ST_TFTOC    (0x02U)     /* testFailedThisOperationCycle   */
#define DEM_ST_PDTC     (0x04U)     /* pendingDTC                     */
#define DEM_ST_CDTC     (0x08U)     /* confirmedDTC                   */
#define DEM_ST_TFSLC    (0x20U)     /* testFailedSinceLastClear       */
#define DEM_STATUS_AVAILABILITY_MASK \
    (DEM_ST_TF | DEM_ST_TFTOC | DEM_ST_PDTC | DEM_ST_CDTC | DEM_ST_TFSLC)

void     dem_init(void);                        /* load from NVM, start operation cycle */
void     dem_report(dem_event_t evt, bool failed);
void     dem_clear_all(void);
void     dem_main_100ms(void);                  /* deferred NVM write */

uint32_t dem_dtc_code(dem_event_t evt);
uint8_t  dem_status(dem_event_t evt);
uint8_t  dem_occurrences(dem_event_t evt);
uint16_t dem_count_by_mask(uint8_t mask);
/** Write [DTC_hi DTC_mid DTC_lo status] records, returns bytes written. */
uint16_t dem_get_by_mask(uint8_t mask, uint8_t *out, uint16_t max);
uint8_t  dem_confirmed_count(void);

#endif /* DEM_H */
