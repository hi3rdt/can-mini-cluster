/**
 * @file    test_com_layer.c
 * @brief   Host tests for the STM32 COM layer: E2E rejection and timeout monitoring.
 *
 * services/com.c is compiled unchanged for the PC; only the bxCAN driver is replaced
 * by fake_bxcan.c, so the tests exercise the same code that runs on the target.
 */
#include "unity.h"
#include "com.h"
#include "e2e.h"
#include "fake_bxcan.h"
#include <string.h>

static uint8_t s_engine_alive;

void setUp(void)
{
    fake_can_reset();
    com_init(0U);
    s_engine_alive = 0U;
}

void tearDown(void) {}

/* Build and queue one E2E-protected EngineData frame */
static void rx_engine(uint16_t speed_x100, bool corrupt_crc)
{
    msg_engine_data_t m = { 0 };
    uint8_t buf[DLC_ENGINE_DATA];

    m.engine_speed_raw  = 3000U * ENGINE_RPM_RAW_PER_RPM;
    m.vehicle_speed_raw = speed_x100;
    m.coolant_temp_raw  = (uint8_t)(90 + COOLANT_OFFSET_C);
    can_pack_engine_data(&m, buf);
    e2e_protect(CANID_ENGINE_DATA, buf, DLC_ENGINE_DATA, ENGINE_DATA_CRC_POS,
                ENGINE_DATA_ALIVE_POS, &s_engine_alive);
    if (corrupt_crc)
    {
        buf[ENGINE_DATA_CRC_POS] ^= 0x01U;
    }
    fake_can_push_rx(CANID_ENGINE_DATA, buf, DLC_ENGINE_DATA);
}

static void test_engine_invalid_before_first_frame(void)
{
    msg_engine_data_t e;
    TEST_ASSERT_FALSE(com_get_engine(&e));
}

static void test_valid_frame_updates_signals(void)
{
    msg_engine_data_t e;

    rx_engine(5000U, false);                /* 50.00 km/h */
    com_poll_rx(10U);
    TEST_ASSERT_TRUE(com_get_engine(&e));
    TEST_ASSERT_EQUAL_UINT16(5000U, e.vehicle_speed_raw);
}

static void test_crc_error_is_dropped_and_counted(void)
{
    msg_engine_data_t e;
    com_stats_t st;

    rx_engine(5000U, true);
    com_poll_rx(10U);
    com_get_stats(&st);
    TEST_ASSERT_FALSE(com_get_engine(&e));
    TEST_ASSERT_EQUAL_UINT32(1U, st.e2e_crc_errors);
    TEST_ASSERT_EQUAL_UINT32(0U, st.engine_rx);
}

static void test_timeout_after_100ms_without_frames(void)
{
    msg_engine_data_t e;
    com_stats_t st;

    rx_engine(5000U, false);
    com_poll_rx(0U);
    com_check_timeouts(99U);
    TEST_ASSERT_TRUE(com_get_engine(&e));   /* 99 ms: still within 5 x 20 ms */
    com_check_timeouts(100U);
    TEST_ASSERT_FALSE(com_get_engine(&e));
    com_get_stats(&st);
    TEST_ASSERT_EQUAL_UINT32(1U, st.engine_timeouts);
}

static void test_corrupted_frames_do_not_restart_timeout(void)
{
    msg_engine_data_t e;

    rx_engine(5000U, false);
    com_poll_rx(0U);
    for (uint32_t t = 20U; t <= 100U; t += 20U)
    {
        rx_engine(5000U, true);             /* sender alive but every CRC wrong */
        com_poll_rx(t);
        com_check_timeouts(t);
    }
    TEST_ASSERT_FALSE(com_get_engine(&e));
}

static void test_recovers_with_first_frame_after_timeout(void)
{
    msg_engine_data_t e;

    rx_engine(5000U, false);
    com_poll_rx(0U);
    com_check_timeouts(200U);
    TEST_ASSERT_FALSE(com_get_engine(&e));

    s_engine_alive = 9U;                    /* sender rebooted: counter jumped */
    rx_engine(6000U, false);
    com_poll_rx(210U);
    TEST_ASSERT_TRUE(com_get_engine(&e));
    TEST_ASSERT_EQUAL_UINT16(6000U, e.vehicle_speed_raw);
}

static void test_timeout_handles_tick_wraparound(void)
{
    msg_engine_data_t e;
    const uint32_t t0 = 0xFFFFFFF0U;        /* 16 ms before the 32-bit counter wraps */

    rx_engine(5000U, false);
    com_poll_rx(t0);
    com_check_timeouts((uint32_t)(t0 + 50U));  /* wraps to 34 */
    TEST_ASSERT_TRUE(com_get_engine(&e));
    com_check_timeouts((uint32_t)(t0 + 100U));
    TEST_ASSERT_FALSE(com_get_engine(&e));
}

static void test_cluster_status_is_protected(void)
{
    msg_cluster_status_t m = { 0 };
    can_frame_t tx;
    e2e_rx_state_t rx = { 0U, false };

    m.fuel_level_pct = 42U;
    m.odometer_raw   = 12345U;
    TEST_ASSERT_EQUAL(E_OK, com_send_cluster_status(&m));
    TEST_ASSERT_EQUAL_UINT32(1U, fake_can_last_tx(&tx));
    TEST_ASSERT_EQUAL_HEX32(CANID_CLUSTER_STATUS, tx.id);
    TEST_ASSERT_EQUAL(E2E_STATUS_INITIAL,
                      e2e_check(CANID_CLUSTER_STATUS, tx.data, DLC_CLUSTER_STATUS,
                                CLUSTER_STATUS_CRC_POS, CLUSTER_STATUS_ALIVE_POS, &rx));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_engine_invalid_before_first_frame);
    RUN_TEST(test_valid_frame_updates_signals);
    RUN_TEST(test_crc_error_is_dropped_and_counted);
    RUN_TEST(test_timeout_after_100ms_without_frames);
    RUN_TEST(test_corrupted_frames_do_not_restart_timeout);
    RUN_TEST(test_recovers_with_first_frame_after_timeout);
    RUN_TEST(test_timeout_handles_tick_wraparound);
    RUN_TEST(test_cluster_status_is_protected);
    return UNITY_END();
}
