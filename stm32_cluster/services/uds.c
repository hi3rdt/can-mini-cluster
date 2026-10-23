/**
 * @file    uds.c
 * @brief   UDS-lite diagnostic server (see uds.h).
 */
#include "uds.h"
#include "dem.h"

#define SID_DSC     (0x10U)
#define SID_ER      (0x11U)
#define SID_CDTCI   (0x14U)
#define SID_RDTCI   (0x19U)
#define SID_RDBI    (0x22U)
#define SID_TP      (0x3EU)
#define SID_NEG     (0x7FU)
#define POS_OFFSET  (0x40U)

#define SUPPRESS_POS_RSP    (0x80U)
#define SUBFUNC_MASK        (0x7FU)
#define RDBI_MAX_DIDS       (4U)

static const uds_cfg_t *s_cfg;
static uint8_t  s_session;
static uint32_t s_last_rx_ms;
static bool     s_reset_pending;
static uint32_t s_reset_at_ms;

typedef struct
{
    const uint8_t *req;
    uint16_t       len;
    uint8_t       *resp;
    uint16_t       max;
    uint16_t       rlen;      /* response length so far */
    uint8_t        nrc;       /* 0 = positive */
    bool           suppress;  /* suppressPosRspMsgIndicationBit set */
} uds_ctx_t;

static bool put(uds_ctx_t *c, uint8_t b)
{
    if (c->rlen >= c->max)
    {
        c->nrc = UDS_NRC_RESPONSE_TOO_LONG;
        return false;
    }
    c->resp[c->rlen] = b;
    c->rlen++;
    return true;
}

static void svc_session_control(uds_ctx_t *c)
{
    if (c->len != 2U)
    {
        c->nrc = UDS_NRC_INCORRECT_LENGTH;
        return;
    }
    const uint8_t sub = (uint8_t)(c->req[1] & SUBFUNC_MASK);
    c->suppress = ((c->req[1] & SUPPRESS_POS_RSP) != 0U);

    if ((sub != UDS_SESSION_DEFAULT) && (sub != UDS_SESSION_EXTENDED))
    {
        c->nrc = UDS_NRC_SUBFUNC_NOT_SUPPORTED;
        return;
    }
    s_session = sub;
    (void)put(c, (uint8_t)(SID_DSC + POS_OFFSET));
    (void)put(c, sub);
    (void)put(c, (uint8_t)(UDS_P2_MS >> 8));
    (void)put(c, (uint8_t)(UDS_P2_MS & 0xFFU));
    (void)put(c, (uint8_t)((UDS_P2_STAR_MS / 10U) >> 8));      /* P2* in 10 ms units */
    (void)put(c, (uint8_t)((UDS_P2_STAR_MS / 10U) & 0xFFU));
}

static void svc_ecu_reset(uds_ctx_t *c, uint32_t now_ms)
{
    if (c->len != 2U)
    {
        c->nrc = UDS_NRC_INCORRECT_LENGTH;
        return;
    }
    const uint8_t sub = (uint8_t)(c->req[1] & SUBFUNC_MASK);
    c->suppress = ((c->req[1] & SUPPRESS_POS_RSP) != 0U);
    if (sub != 0x01U)                                          /* hardReset only */
    {
        c->nrc = UDS_NRC_SUBFUNC_NOT_SUPPORTED;
        return;
    }
    (void)put(c, (uint8_t)(SID_ER + POS_OFFSET));
    (void)put(c, sub);
    s_reset_pending = true;
    s_reset_at_ms   = now_ms + UDS_RESET_DELAY_MS;
}

static void svc_clear_dtc(uds_ctx_t *c)
{
    if (c->len != 4U)
    {
        c->nrc = UDS_NRC_INCORRECT_LENGTH;
        return;
    }
    if (s_session != UDS_SESSION_EXTENDED)
    {
        c->nrc = UDS_NRC_SERVICE_NOT_IN_SESSION;
        return;
    }
    const uint32_t group = ((uint32_t)c->req[1] << 16) | ((uint32_t)c->req[2] << 8) | c->req[3];
    if (group != 0xFFFFFFUL)                                   /* all groups only */
    {
        c->nrc = UDS_NRC_REQUEST_OUT_OF_RANGE;
        return;
    }
    dem_clear_all();
    (void)put(c, (uint8_t)(SID_CDTCI + POS_OFFSET));
}

static void svc_read_dtc(uds_ctx_t *c)
{
    if (c->len < 2U)
    {
        c->nrc = UDS_NRC_INCORRECT_LENGTH;
        return;
    }
    const uint8_t sub = (uint8_t)(c->req[1] & SUBFUNC_MASK);
    c->suppress = ((c->req[1] & SUPPRESS_POS_RSP) != 0U);

    if ((sub != 0x01U) && (sub != 0x02U))
    {
        c->nrc = UDS_NRC_SUBFUNC_NOT_SUPPORTED;
        return;
    }
    if (c->len != 3U)
    {
        c->nrc = UDS_NRC_INCORRECT_LENGTH;
        return;
    }
    const uint8_t mask = c->req[2];

    (void)put(c, (uint8_t)(SID_RDTCI + POS_OFFSET));
    (void)put(c, sub);
    (void)put(c, DEM_STATUS_AVAILABILITY_MASK);

    if (sub == 0x01U)                                          /* reportNumberOfDTCByStatusMask */
    {
        const uint16_t n = dem_count_by_mask(mask);
        (void)put(c, 0x01U);                                   /* DTCFormat ISO 14229-1 */
        (void)put(c, (uint8_t)(n >> 8));
        (void)put(c, (uint8_t)(n & 0xFFU));
    }
    else                                                       /* reportDTCByStatusMask */
    {
        const uint16_t n = dem_get_by_mask(mask, &c->resp[c->rlen],
                                           (uint16_t)(c->max - c->rlen));
        c->rlen = (uint16_t)(c->rlen + n);
    }
}

static void svc_read_did(uds_ctx_t *c)
{
    /* 0x22 + 1..RDBI_MAX_DIDS identifiers of 2 bytes each */
    if ((c->len < 3U) || (((c->len - 1U) % 2U) != 0U) ||
        (((c->len - 1U) / 2U) > RDBI_MAX_DIDS))
    {
        c->nrc = UDS_NRC_INCORRECT_LENGTH;
        return;
    }
    (void)put(c, (uint8_t)(SID_RDBI + POS_OFFSET));

    bool any = false;
    for (uint16_t i = 1U; i < c->len; i = (uint16_t)(i + 2U))   /* len is odd: i + 1 < len */
    {
        const uint16_t did = (uint16_t)(((uint16_t)c->req[i] << 8) | c->req[i + 1U]);
        if ((uint16_t)(c->rlen + 2U) > c->max)
        {
            c->nrc = UDS_NRC_RESPONSE_TOO_LONG;
            return;
        }
        const uint16_t room = (uint16_t)(c->max - c->rlen - 2U);
        const uint16_t n = s_cfg->read_did(did, &c->resp[c->rlen + 2U], room);
        if (n > 0U)
        {
            c->resp[c->rlen]      = c->req[i];
            c->resp[c->rlen + 1U] = c->req[i + 1U];
            c->rlen = (uint16_t)(c->rlen + 2U + n);
            any = true;
        }
    }
    if (!any)
    {
        c->nrc = UDS_NRC_REQUEST_OUT_OF_RANGE;   /* none of the DIDs is supported */
    }
}

static void svc_tester_present(uds_ctx_t *c)
{
    if (c->len != 2U)
    {
        c->nrc = UDS_NRC_INCORRECT_LENGTH;
        return;
    }
    if ((c->req[1] & SUBFUNC_MASK) != 0x00U)
    {
        c->nrc = UDS_NRC_SUBFUNC_NOT_SUPPORTED;
        return;
    }
    c->suppress = ((c->req[1] & SUPPRESS_POS_RSP) != 0U);
    (void)put(c, (uint8_t)(SID_TP + POS_OFFSET));
    (void)put(c, 0x00U);
}

void uds_init(const uds_cfg_t *cfg)
{
    s_cfg           = cfg;
    s_session       = UDS_SESSION_DEFAULT;
    s_last_rx_ms    = 0U;
    s_reset_pending = false;
}

uint16_t uds_process(const uint8_t *req, uint16_t len, bool functional,
                     uint8_t *resp, uint16_t resp_max, uint32_t now_ms)
{
    uds_ctx_t c = { req, len, resp, resp_max, 0U, 0U, false };

    if ((len == 0U) || (resp_max < 3U))
    {
        return 0U;
    }
    s_last_rx_ms = now_ms;          /* any request restarts S3 */

    switch (req[0])
    {
    case SID_DSC:   svc_session_control(&c);   break;
    case SID_ER:    svc_ecu_reset(&c, now_ms); break;
    case SID_CDTCI: svc_clear_dtc(&c);         break;
    case SID_RDTCI: svc_read_dtc(&c);          break;
    case SID_RDBI:  svc_read_did(&c);          break;
    case SID_TP:    svc_tester_present(&c);    break;
    default:        c.nrc = UDS_NRC_SERVICE_NOT_SUPPORTED; break;
    }

    if (c.nrc != 0U)
    {
        /* ISO 14229-1 7.5: these NRCs are not sent for functionally addressed requests */
        if (functional && ((c.nrc == UDS_NRC_SERVICE_NOT_SUPPORTED) ||
                           (c.nrc == UDS_NRC_SUBFUNC_NOT_SUPPORTED) ||
                           (c.nrc == UDS_NRC_REQUEST_OUT_OF_RANGE)))
        {
            return 0U;
        }
        resp[0] = SID_NEG;
        resp[1] = req[0];
        resp[2] = c.nrc;
        return 3U;
    }
    return c.suppress ? 0U : c.rlen;
}

void uds_main(uint32_t now_ms)
{
    if ((s_session != UDS_SESSION_DEFAULT) &&
        ((uint32_t)(now_ms - s_last_rx_ms) >= UDS_S3_TIMEOUT_MS))
    {
        s_session = UDS_SESSION_DEFAULT;   /* S3 server timeout */
    }
    if (s_reset_pending && ((int32_t)(now_ms - s_reset_at_ms) >= 0))
    {
        s_reset_pending = false;
        s_cfg->hard_reset();
    }
}

uint8_t uds_session(void)
{
    return s_session;
}
