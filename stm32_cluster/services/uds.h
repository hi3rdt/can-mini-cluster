/**
 * @file    uds.h
 * @brief   UDS-lite diagnostic server (ISO 14229-1 subset), transport independent.
 *
 * Supported: 0x10 DiagnosticSessionControl, 0x11 ECUReset, 0x14 ClearDTC,
 *            0x19 ReadDTCInformation (0x01, 0x02), 0x22 ReadDataByIdentifier,
 *            0x3E TesterPresent.
 */
#ifndef UDS_H
#define UDS_H

#include "std_types.h"

#define UDS_SESSION_DEFAULT     (0x01U)
#define UDS_SESSION_EXTENDED    (0x03U)

#define UDS_S3_TIMEOUT_MS       (5000U)
#define UDS_P2_MS               (50U)
#define UDS_P2_STAR_MS          (5000U)
#define UDS_RESET_DELAY_MS      (50U)   /* let the positive response leave the bus */

/* Negative response codes */
#define UDS_NRC_GENERAL_REJECT          (0x10U)
#define UDS_NRC_SERVICE_NOT_SUPPORTED   (0x11U)
#define UDS_NRC_SUBFUNC_NOT_SUPPORTED   (0x12U)
#define UDS_NRC_INCORRECT_LENGTH        (0x13U)
#define UDS_NRC_RESPONSE_TOO_LONG       (0x14U)
#define UDS_NRC_CONDITIONS_NOT_CORRECT  (0x22U)
#define UDS_NRC_REQUEST_OUT_OF_RANGE    (0x31U)
#define UDS_NRC_SERVICE_NOT_IN_SESSION  (0x7FU)

typedef struct
{
    /** Copy DID data into out, return its length or 0 when the DID is not supported. */
    uint16_t (*read_did)(uint16_t did, uint8_t *out, uint16_t max);
    /** Perform the hardware reset (called from uds_main, after the response). */
    void (*hard_reset)(void);
} uds_cfg_t;

void     uds_init(const uds_cfg_t *cfg);
/**
 * Process one request. Returns the response length (0 = no response, e.g.
 * suppressPosRspMsgIndicationBit or a suppressed NRC on functional addressing).
 */
uint16_t uds_process(const uint8_t *req, uint16_t len, bool functional,
                     uint8_t *resp, uint16_t resp_max, uint32_t now_ms);
void     uds_main(uint32_t now_ms);
uint8_t  uds_session(void);

#endif /* UDS_H */
