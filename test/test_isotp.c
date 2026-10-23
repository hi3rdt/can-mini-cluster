/**
 * @file    test_isotp.c
 * @brief   Host tests: two ISO-TP channels (tester <-> ECU) wired through a fake CAN bus.
 */
#include "unity.h"
#include "isotp.h"
#include <string.h>

#define BUS_DEPTH   (64U)

typedef struct { uint32_t id; uint8_t data[8]; } frame_t;

static frame_t  s_bus[BUS_DEPTH];
static uint32_t s_bus_n;
static uint32_t s_now;

static uint8_t  s_ecu_rx[128], s_ecu_tx[128], s_tst_rx[128], s_tst_tx[128];
static uint8_t  s_got[128];
static uint16_t s_got_len;
static uint32_t s_got_count;

static void bus_tx(uint32_t id, const uint8_t data[8], void *ctx)
{
    (void)ctx;
    TEST_ASSERT_TRUE(s_bus_n < BUS_DEPTH);
    s_bus[s_bus_n].id = id;
    (void)memcpy(s_bus[s_bus_n].data, data, 8);
    s_bus_n++;
}

static void on_rx(const uint8_t *data, uint16_t len, void *ctx)
{
    (void)ctx;
    (void)memcpy(s_got, data, len);
    s_got_len = len;
    s_got_count++;
}

static isotp_cfg_t s_ecu_cfg, s_tst_cfg;
static isotp_t s_ecu, s_tst;

/* Deliver every queued frame to the channel listening on its ID, run both mains, repeat */
static void run_bus(uint32_t ms)
{
    for (uint32_t t = 0U; t < ms; t++)
    {
        uint32_t i = 0U;
        while (i < s_bus_n)
        {
            frame_t f = s_bus[i];
            i++;
            if (f.id == s_ecu_cfg.rx_id) { isotp_on_can_rx(&s_ecu, f.data, 8U, s_now); }
            if (f.id == s_tst_cfg.rx_id) { isotp_on_can_rx(&s_tst, f.data, 8U, s_now); }
        }
        s_bus_n = 0U;
        isotp_main(&s_ecu, s_now);
        isotp_main(&s_tst, s_now);
        s_now++;
    }
}

void setUp(void)
{
    s_bus_n = 0U; s_now = 1000U; s_got_len = 0U; s_got_count = 0U;
    s_ecu_cfg = (isotp_cfg_t){ 0x7E8U, 0x7E0U, 0U, 0U, s_ecu_rx, sizeof(s_ecu_rx),
                               s_ecu_tx, sizeof(s_ecu_tx), bus_tx, on_rx, NULL };
    s_tst_cfg = (isotp_cfg_t){ 0x7E0U, 0x7E8U, 4U, 2U, s_tst_rx, sizeof(s_tst_rx),
                               s_tst_tx, sizeof(s_tst_tx), bus_tx, on_rx, NULL };
    isotp_init(&s_ecu, &s_ecu_cfg);
    isotp_init(&s_tst, &s_tst_cfg);
}
void tearDown(void) {}

static void test_single_frame_padded(void)
{
    const uint8_t req[] = { 0x22, 0xF1, 0x90 };
    TEST_ASSERT_EQUAL(E_OK, isotp_send(&s_tst, req, 3U, s_now));
    TEST_ASSERT_EQUAL_UINT32(1U, s_bus_n);
    const uint8_t expect[8] = { 0x03, 0x22, 0xF1, 0x90, 0xCC, 0xCC, 0xCC, 0xCC };
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expect, s_bus[0].data, 8);
    run_bus(1U);
    TEST_ASSERT_EQUAL_UINT16(3U, s_got_len);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(req, s_got, 3);
}

static void test_multi_frame_with_block_size_and_stmin(void)
{
    uint8_t msg[100];
    for (uint8_t i = 0U; i < sizeof(msg); i++) { msg[i] = i; }

    /* ECU -> tester: tester announces BS=4, STmin=2 ms */
    TEST_ASSERT_EQUAL(E_OK, isotp_send(&s_ecu, msg, sizeof(msg), s_now));
    TEST_ASSERT_TRUE(isotp_tx_busy(&s_ecu));
    TEST_ASSERT_EQUAL(E_BUSY, isotp_send(&s_ecu, msg, 10U, s_now));
    run_bus(200U);
    TEST_ASSERT_FALSE(isotp_tx_busy(&s_ecu));
    TEST_ASSERT_EQUAL_UINT32(1U, s_got_count);
    TEST_ASSERT_EQUAL_UINT16(sizeof(msg), s_got_len);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(msg, s_got, sizeof(msg));
    TEST_ASSERT_EQUAL_UINT16(0U, s_ecu.stats.tx_errors);
}

static void test_stmin_is_respected(void)
{
    uint8_t msg[30] = { 0 };
    TEST_ASSERT_EQUAL(E_OK, isotp_send(&s_ecu, msg, sizeof(msg), s_now));  /* FF + 4 CF */
    run_bus(1U);                       /* FF delivered, FC sent */
    run_bus(1U);                       /* FC delivered, first CF due */
    run_bus(1U);
    TEST_ASSERT_EQUAL_UINT32(0U, s_got_count);
    run_bus(20U);
    TEST_ASSERT_EQUAL_UINT32(1U, s_got_count);
}

static void test_n_bs_timeout_without_flow_control(void)
{
    uint8_t msg[20] = { 0 };
    TEST_ASSERT_EQUAL(E_OK, isotp_send(&s_ecu, msg, sizeof(msg), s_now));
    s_bus_n = 0U;                      /* FF lost: no FC will ever come */
    for (uint32_t t = 0U; t < ISOTP_N_BS_MS + 1U; t++) { isotp_main(&s_ecu, s_now++); }
    TEST_ASSERT_FALSE(isotp_tx_busy(&s_ecu));
    TEST_ASSERT_EQUAL_UINT16(1U, s_ecu.stats.tx_errors);
}

static void test_wrong_sequence_number_aborts(void)
{
    const uint8_t ff[8]  = { 0x10, 0x14, 1, 2, 3, 4, 5, 6 };   /* 20 bytes */
    const uint8_t cf2[8] = { 0x22, 7, 8, 9, 10, 11, 12, 13 };  /* SN 2, expected 1 */
    isotp_on_can_rx(&s_ecu, ff, 8U, s_now);
    isotp_on_can_rx(&s_ecu, cf2, 8U, s_now);
    TEST_ASSERT_EQUAL_UINT16(1U, s_ecu.stats.rx_errors);
    TEST_ASSERT_EQUAL_UINT32(0U, s_got_count);
}

static void test_overflow_answers_fc_overflow(void)
{
    const uint8_t ff[8] = { 0x11, 0x00, 1, 2, 3, 4, 5, 6 };    /* 256 > 128 byte buffer */
    isotp_on_can_rx(&s_ecu, ff, 8U, s_now);
    TEST_ASSERT_EQUAL_UINT32(1U, s_bus_n);
    TEST_ASSERT_EQUAL_HEX8(0x32, s_bus[0].data[0]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_single_frame_padded);
    RUN_TEST(test_multi_frame_with_block_size_and_stmin);
    RUN_TEST(test_stmin_is_respected);
    RUN_TEST(test_n_bs_timeout_without_flow_control);
    RUN_TEST(test_wrong_sequence_number_aborts);
    RUN_TEST(test_overflow_answers_fc_overflow);
    return UNITY_END();
}
