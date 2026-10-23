/**
 * @file    bxcan.c
 * @brief   bxCAN driver (see bxcan.h).
 *
 * Bit timing: PCLK1 = 36 MHz, BRP = 4 -> tq = 111.1 ns, 18 tq/bit
 * (SYNC 1 + BS1 14 + BS2 3) = 500 kbit/s, sample point 15/18 = 83.3 %, SJW 2 tq.
 * The ESP32 node samples at 85 % (20 tq); see esp32_vehicle_sim/main/can_if.c.
 *
 * Filter bank 0, 16-bit identifier-list mode, accepts exactly:
 * 0x100 EngineData and 0x200 BodyStatus (each listed twice: a bank holds 4 IDs).
 * Every other frame is dropped by hardware and never costs CPU time.
 */
#include "bxcan.h"
#include "gpio.h"
#include "systick.h"
#include "board.h"
#include <string.h>

#define CAN_INIT_TIMEOUT_MS (20UL)
#define CAN_STD16(id)       ((uint32_t)(id) << 5)   /* STID[10:0] in bits 15..5 */
#define RXQ_MASK            (CAN_RXQ_SIZE - 1U)

static can_frame_t       s_rxq[CAN_RXQ_SIZE];
static volatile uint16_t s_rxq_head;    /* ISR  */
static volatile uint16_t s_rxq_tail;    /* main */
static volatile can_stats_t s_stats;
static volatile bool     s_busoff_event;

static bool wait_inak(bool expect)
{
    const uint32_t t0 = tick_get();
    while (((CAN1->MSR & CAN_MSR_INAK) != 0UL) != expect)
    {
        if (tick_elapsed(t0, CAN_INIT_TIMEOUT_MS))
        {
            return false;
        }
    }
    return true;
}

std_return_t can_init(can_mode_t mode)
{
    RCC->APB1ENR |= RCC_APB1ENR_CAN1EN;

    /* Remap 2: RX = PB8, TX = PB9. SWJ_CFG bits read as 0 so the RMW keeps full SWD. */
    AFIO->MAPR = (AFIO->MAPR & ~AFIO_MAPR_CAN_REMAP) | AFIO_MAPR_CAN_REMAP_REMAP2;
    gpio_config_pullup(GPIOB, 8U);
    gpio_config(GPIOB, 9U, GPIO_AF_PP_50MHZ);

    CAN1->MCR &= ~CAN_MCR_SLEEP;                    /* bxCAN leaves reset in sleep mode */
    CAN1->MCR |= CAN_MCR_INRQ;
    if (!wait_inak(true))
    {
        return E_TIMEOUT;
    }

    CAN1->MCR |= CAN_MCR_ABOM      /* leave bus-off automatically after 128 x 11 recessive bits */
               | CAN_MCR_TXFP;     /* transmit in request order, not by identifier */

    CAN1->BTR = (1UL  << CAN_BTR_SJW_Pos)           /* SJW = 2 tq  */
              | (2UL  << CAN_BTR_TS2_Pos)           /* BS2 = 3 tq  */
              | (13UL << CAN_BTR_TS1_Pos)           /* BS1 = 14 tq */
              | (3UL  << CAN_BTR_BRP_Pos)           /* BRP = 4     */
              | ((mode == CAN_MODE_LOOPBACK_SILENT) ? (CAN_BTR_LBKM | CAN_BTR_SILM) : 0UL);

    CAN1->FMR   |= CAN_FMR_FINIT;
    CAN1->FA1R  &= ~CAN_FA1R_FACT0;
    CAN1->FS1R  &= ~CAN_FS1R_FSC0;                  /* 16-bit scale */
    CAN1->FM1R  |= CAN_FM1R_FBM0;                   /* identifier list */
    CAN1->FFA1R &= ~CAN_FFA1R_FFA0;                 /* -> FIFO0 */
    CAN1->sFilterRegister[0].FR1 = (CAN_STD16(0x200U) << 16) | CAN_STD16(0x100U);
    CAN1->sFilterRegister[0].FR2 = (CAN_STD16(0x200U) << 16) | CAN_STD16(0x100U);
    CAN1->FA1R  |= CAN_FA1R_FACT0;
    CAN1->FMR   &= ~CAN_FMR_FINIT;

    CAN1->IER = CAN_IER_FMPIE0 | CAN_IER_FOVIE0 | CAN_IER_ERRIE | CAN_IER_BOFIE | CAN_IER_EPVIE;
    NVIC_SetPriority(USB_LP_CAN1_RX0_IRQn, IRQ_PRIO_CAN_RX);
    NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);
    NVIC_SetPriority(CAN1_SCE_IRQn, IRQ_PRIO_CAN_SCE);
    NVIC_EnableIRQ(CAN1_SCE_IRQn);

    CAN1->MCR &= ~CAN_MCR_INRQ;
    /* Leaves init only after 11 recessive bits: times out if RX floats or the bus is shorted */
    return wait_inak(false) ? E_OK : E_TIMEOUT;
}

std_return_t can_send(uint32_t id, const uint8_t *data, uint8_t dlc)
{
    const uint32_t tsr = CAN1->TSR;
    uint32_t mb;

    if (dlc > 8U)
    {
        return E_NOT_OK;
    }
    if ((tsr & (CAN_TSR_TME0 | CAN_TSR_TME1 | CAN_TSR_TME2)) == 0UL)
    {
        s_stats.tx_no_mailbox++;
        return E_BUSY;
    }
    mb = (tsr & CAN_TSR_CODE) >> CAN_TSR_CODE_Pos;  /* hardware suggests a free mailbox */

    uint8_t b[8] = { 0 };
    (void)memcpy(b, data, dlc);

    CAN1->sTxMailBox[mb].TDTR = dlc;
    CAN1->sTxMailBox[mb].TDLR = (uint32_t)b[0] | ((uint32_t)b[1] << 8) |
                                ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
    CAN1->sTxMailBox[mb].TDHR = (uint32_t)b[4] | ((uint32_t)b[5] << 8) |
                                ((uint32_t)b[6] << 16) | ((uint32_t)b[7] << 24);
    CAN1->sTxMailBox[mb].TIR  = (id << CAN_TI0R_STID_Pos) | CAN_TI0R_TXRQ;   /* request last */
    s_stats.tx_frames++;
    return E_OK;
}

bool can_receive(can_frame_t *f)
{
    const uint16_t tail = s_rxq_tail;
    if (tail == s_rxq_head)
    {
        return false;
    }
    *f = s_rxq[tail];
    s_rxq_tail = (uint16_t)((tail + 1U) & RXQ_MASK);
    return true;
}

void can_get_stats(can_stats_t *out)
{
    const uint32_t esr = CAN1->ESR;

    __disable_irq();
    *out = *(const can_stats_t *)&s_stats;          /* snapshot, ISR updates counters */
    __enable_irq();

    out->tec             = (uint8_t)((esr & CAN_ESR_TEC) >> CAN_ESR_TEC_Pos);
    out->rec             = (uint8_t)((esr & CAN_ESR_REC) >> CAN_ESR_REC_Pos);
    out->last_error_code = (uint8_t)((esr & CAN_ESR_LEC) >> CAN_ESR_LEC_Pos);
    out->error_passive   = ((esr & CAN_ESR_EPVF) != 0UL);
    out->bus_off         = ((esr & CAN_ESR_BOFF) != 0UL);
}

bool can_take_busoff_event(void)
{
    bool ev = false;
    __disable_irq();
    if (s_busoff_event)
    {
        s_busoff_event = false;
        ev = true;
    }
    __enable_irq();
    return ev;
}

void USB_LP_CAN1_RX0_IRQHandler(void)
{
    if ((CAN1->RF0R & CAN_RF0R_FOVR0) != 0UL)
    {
        s_stats.rx_fifo_overruns++;
        CAN1->RF0R = CAN_RF0R_FOVR0;                /* rc_w1 */
    }

    while ((CAN1->RF0R & CAN_RF0R_FMP0) != 0UL)
    {
        const uint32_t rir  = CAN1->sFIFOMailBox[0].RIR;
        const uint32_t rdtr = CAN1->sFIFOMailBox[0].RDTR;
        const uint32_t rdlr = CAN1->sFIFOMailBox[0].RDLR;
        const uint32_t rdhr = CAN1->sFIFOMailBox[0].RDHR;

        /* Plain write, never |= : FULL0/FOVR0 are rc_w1 and would be cleared by accident */
        CAN1->RF0R = CAN_RF0R_RFOM0;

        const uint16_t head = s_rxq_head;
        const uint16_t next = (uint16_t)((head + 1U) & RXQ_MASK);
        if (next == s_rxq_tail)
        {
            s_stats.rx_queue_overflows++;
            continue;
        }

        can_frame_t *f = &s_rxq[head];
        f->id  = ((rir & CAN_RI0R_IDE) != 0UL) ? (rir >> CAN_RI0R_EXID_Pos)
                                               : (rir >> CAN_RI0R_STID_Pos);
        f->dlc = (uint8_t)(rdtr & CAN_RDT0R_DLC);
        if (f->dlc > 8U)
        {
            f->dlc = 8U;                             /* DLC 9..15 still means 8 bytes */
        }
        for (uint8_t i = 0U; i < 4U; i++)
        {
            f->data[i]      = (uint8_t)(rdlr >> (8U * i));
            f->data[i + 4U] = (uint8_t)(rdhr >> (8U * i));
        }
        s_rxq_head = next;
        s_stats.rx_frames++;
    }
}

void CAN1_SCE_IRQHandler(void)
{
    const uint32_t esr = CAN1->ESR;

    /* ERRI is raised on entry into error-passive (EPVIE) and bus-off (BOFIE), once per
       transition, so each interrupt with BOFF set is one new bus-off event. */
    if ((esr & CAN_ESR_BOFF) != 0UL)
    {
        s_stats.bus_off_events++;
        s_busoff_event = true;
    }
    CAN1->MSR = CAN_MSR_ERRI;                        /* rc_w1: acknowledge the error interrupt */
}
