/**
 * @file    isotp.c
 * @brief   ISO 15765-2 transport layer (see isotp.h).
 */
#include "isotp.h"
#include <string.h>

#define PCI_SF      (0x0U)
#define PCI_FF      (0x1U)
#define PCI_CF      (0x2U)
#define PCI_FC      (0x3U)

#define FC_CTS      (0x0U)
#define FC_WAIT     (0x1U)
#define FC_OVFLW    (0x2U)

#define SF_MAX_DL   (7U)
#define FF_DATA     (6U)
#define CF_DATA     (7U)

/* Wrap-safe "now has reached deadline" */
static bool time_reached(uint32_t now_ms, uint32_t deadline_ms)
{
    return (int32_t)(now_ms - deadline_ms) >= 0;
}

/* STmin byte -> milliseconds; 100..900 us values are rounded up to 1 ms */
static uint8_t st_min_decode(uint8_t raw)
{
    uint8_t ms;
    if (raw <= 0x7FU)
    {
        ms = raw;
    }
    else if ((raw >= 0xF1U) && (raw <= 0xF9U))
    {
        ms = 1U;
    }
    else
    {
        ms = 0x7FU;   /* reserved values: use the maximum as ISO 15765-2 requires */
    }
    return ms;
}

static void send_frame(const isotp_t *ch, const uint8_t *payload, uint8_t len)
{
    uint8_t frame[8];
    (void)memset(frame, ISOTP_PADDING, sizeof(frame));
    (void)memcpy(frame, payload, len);
    ch->cfg->can_tx(ch->cfg->tx_id, frame, ch->cfg->ctx);
}

static void send_fc(const isotp_t *ch, uint8_t status)
{
    const uint8_t fc[3] = {
        (uint8_t)((PCI_FC << 4) | status),
        ch->cfg->block_size,
        ch->cfg->st_min_ms
    };
    send_frame(ch, fc, (uint8_t)sizeof(fc));
}

static void send_next_cf(isotp_t *ch, uint32_t now_ms)
{
    uint8_t cf[8];
    uint16_t remaining = (uint16_t)(ch->tx_len - ch->tx_offset);
    uint8_t  n = (remaining > CF_DATA) ? (uint8_t)CF_DATA : (uint8_t)remaining;

    cf[0] = (uint8_t)((PCI_CF << 4) | (ch->tx_sn & 0x0FU));
    (void)memcpy(&cf[1], &ch->cfg->tx_buf[ch->tx_offset], n);
    send_frame(ch, cf, (uint8_t)(n + 1U));

    ch->tx_offset = (uint16_t)(ch->tx_offset + n);
    ch->tx_sn     = (uint8_t)((ch->tx_sn + 1U) & 0x0FU);

    if (ch->tx_offset >= ch->tx_len)
    {
        ch->tx_state = ISOTP_TX_IDLE;
        ch->stats.tx_ok++;
    }
    else if (ch->tx_bs_limited && (--ch->tx_bs_left == 0U))
    {
        ch->tx_state    = ISOTP_TX_WAIT_FC;
        ch->tx_timer_ms = now_ms + ISOTP_N_BS_MS;
    }
    else
    {
        ch->tx_timer_ms = now_ms + ch->tx_st_min_ms;
    }
}

static void handle_fc(isotp_t *ch, const uint8_t *data, uint8_t dlc, uint32_t now_ms)
{
    if ((ch->tx_state != ISOTP_TX_WAIT_FC) || (dlc < 3U))
    {
        return;   /* unexpected FC is ignored (ISO 15765-2 9.8.3) */
    }

    switch (data[0] & 0x0FU)
    {
    case FC_CTS:
        ch->tx_bs_limited = (data[1] != 0U);
        ch->tx_bs_left    = data[1];
        ch->tx_st_min_ms  = st_min_decode(data[2]);
        ch->tx_state      = ISOTP_TX_SEND_CF;
        ch->tx_timer_ms   = now_ms;          /* first CF may follow immediately */
        break;
    case FC_WAIT:
        ch->tx_timer_ms = now_ms + ISOTP_N_BS_MS;
        break;
    default:                                  /* OVFLW or invalid: abort */
        ch->tx_state = ISOTP_TX_IDLE;
        ch->stats.tx_errors++;
        break;
    }
}

static void handle_sf(isotp_t *ch, const uint8_t *data, uint8_t dlc)
{
    const uint8_t sf_dl = (uint8_t)(data[0] & 0x0FU);

    if ((sf_dl == 0U) || (sf_dl > SF_MAX_DL) || (sf_dl > (uint8_t)(dlc - 1U)) ||
        (sf_dl > ch->cfg->rx_buf_size))
    {
        return;
    }
    ch->rx_state = ISOTP_RX_IDLE;             /* a new SF aborts any reception */
    (void)memcpy(ch->cfg->rx_buf, &data[1], sf_dl);
    ch->stats.rx_ok++;
    ch->cfg->rx_done(ch->cfg->rx_buf, sf_dl, ch->cfg->ctx);
}

static void handle_ff(isotp_t *ch, const uint8_t *data, uint8_t dlc, uint32_t now_ms)
{
    const uint16_t ff_dl = (uint16_t)(((uint16_t)(data[0] & 0x0FU) << 8) | data[1]);

    if ((dlc < 8U) || (ff_dl <= SF_MAX_DL))
    {
        return;   /* malformed FF is ignored */
    }
    if (ff_dl > ch->cfg->rx_buf_size)
    {
        ch->rx_state = ISOTP_RX_IDLE;
        ch->stats.rx_errors++;
        send_fc(ch, FC_OVFLW);
        return;
    }

    (void)memcpy(ch->cfg->rx_buf, &data[2], FF_DATA);
    ch->rx_len      = ff_dl;
    ch->rx_offset   = FF_DATA;
    ch->rx_sn       = 1U;
    ch->rx_bs_count = 0U;
    ch->rx_state    = ISOTP_RX_WAIT_CF;
    ch->rx_timer_ms = now_ms + ISOTP_N_CR_MS;
    send_fc(ch, FC_CTS);
}

static void handle_cf(isotp_t *ch, const uint8_t *data, uint8_t dlc, uint32_t now_ms)
{
    if (ch->rx_state != ISOTP_RX_WAIT_CF)
    {
        return;
    }
    if ((data[0] & 0x0FU) != ch->rx_sn)
    {
        ch->rx_state = ISOTP_RX_IDLE;         /* N_WRONG_SN: abort reception */
        ch->stats.rx_errors++;
        return;
    }

    const uint16_t remaining = (uint16_t)(ch->rx_len - ch->rx_offset);
    uint8_t n = (remaining > CF_DATA) ? (uint8_t)CF_DATA : (uint8_t)remaining;
    if (n > (uint8_t)(dlc - 1U))
    {
        n = (uint8_t)(dlc - 1U);              /* tolerate short (unpadded) last CF */
    }
    (void)memcpy(&ch->cfg->rx_buf[ch->rx_offset], &data[1], n);
    ch->rx_offset   = (uint16_t)(ch->rx_offset + n);
    ch->rx_sn       = (uint8_t)((ch->rx_sn + 1U) & 0x0FU);
    ch->rx_timer_ms = now_ms + ISOTP_N_CR_MS;

    if (ch->rx_offset >= ch->rx_len)
    {
        ch->rx_state = ISOTP_RX_IDLE;
        ch->stats.rx_ok++;
        ch->cfg->rx_done(ch->cfg->rx_buf, ch->rx_len, ch->cfg->ctx);
    }
    else if (ch->cfg->block_size != 0U)
    {
        ch->rx_bs_count++;
        if (ch->rx_bs_count >= ch->cfg->block_size)
        {
            ch->rx_bs_count = 0U;
            send_fc(ch, FC_CTS);
        }
    }
    else
    {
        /* BS = 0: sender streams all CFs without further FlowControl */
    }
}

void isotp_init(isotp_t *ch, const isotp_cfg_t *cfg)
{
    (void)memset(ch, 0, sizeof(*ch));
    ch->cfg = cfg;
}

void isotp_on_can_rx(isotp_t *ch, const uint8_t *data, uint8_t dlc, uint32_t now_ms)
{
    if ((dlc == 0U) || (dlc > 8U))
    {
        return;
    }

    switch (data[0] >> 4)
    {
    case PCI_SF: handle_sf(ch, data, dlc);         break;
    case PCI_FF: handle_ff(ch, data, dlc, now_ms); break;
    case PCI_CF: handle_cf(ch, data, dlc, now_ms); break;
    case PCI_FC: handle_fc(ch, data, dlc, now_ms); break;
    default:                                       break;
    }
}

std_return_t isotp_send(isotp_t *ch, const uint8_t *data, uint16_t len, uint32_t now_ms)
{
    if (ch->tx_state != ISOTP_TX_IDLE)
    {
        return E_BUSY;
    }
    if ((len == 0U) || (len > ch->cfg->tx_buf_size) || (len > ISOTP_MAX_PAYLOAD))
    {
        return E_NOT_OK;
    }

    if (len <= SF_MAX_DL)
    {
        uint8_t sf[8];
        sf[0] = (uint8_t)((PCI_SF << 4) | len);
        (void)memcpy(&sf[1], data, len);
        send_frame(ch, sf, (uint8_t)(len + 1U));
        ch->stats.tx_ok++;
        return E_OK;
    }

    (void)memcpy(ch->cfg->tx_buf, data, len);
    uint8_t ff[8];
    ff[0] = (uint8_t)((PCI_FF << 4) | ((len >> 8) & 0x0FU));
    ff[1] = (uint8_t)(len & 0xFFU);
    (void)memcpy(&ff[2], data, FF_DATA);
    send_frame(ch, ff, 8U);

    ch->tx_len      = len;
    ch->tx_offset   = FF_DATA;
    ch->tx_sn       = 1U;
    ch->tx_state    = ISOTP_TX_WAIT_FC;
    ch->tx_timer_ms = now_ms + ISOTP_N_BS_MS;
    return E_OK;
}

void isotp_main(isotp_t *ch, uint32_t now_ms)
{
    if ((ch->rx_state == ISOTP_RX_WAIT_CF) && time_reached(now_ms, ch->rx_timer_ms))
    {
        ch->rx_state = ISOTP_RX_IDLE;         /* N_Cr timeout */
        ch->stats.rx_errors++;
    }

    if (ch->tx_state == ISOTP_TX_WAIT_FC)
    {
        if (time_reached(now_ms, ch->tx_timer_ms))
        {
            ch->tx_state = ISOTP_TX_IDLE;     /* N_Bs timeout */
            ch->stats.tx_errors++;
        }
    }
    else if (ch->tx_state == ISOTP_TX_SEND_CF)
    {
        if (time_reached(now_ms, ch->tx_timer_ms))
        {
            send_next_cf(ch, now_ms);         /* one CF per call keeps the bus load bounded */
        }
    }
    else
    {
        /* idle */
    }
}

bool isotp_tx_busy(const isotp_t *ch)
{
    return ch->tx_state != ISOTP_TX_IDLE;
}
