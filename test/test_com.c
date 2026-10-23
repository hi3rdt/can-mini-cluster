/**
 * @file    test_com.c
 * @brief   Host tests: E2E CRC/alive counter and CAN matrix pack/unpack vs. cantools vectors.
 */
#include "unity.h"
#include "e2e.h"
#include "can_matrix.h"
#include "can_vectors.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_crc8_j1850_check_value(void)
{
    const uint8_t check[] = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };
    TEST_ASSERT_EQUAL_HEX8(0x4B, e2e_crc8(check, sizeof(check), 0x00U));
}

static void test_crc8_chaining_equals_single_pass(void)
{
    const uint8_t check[] = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };
    const uint8_t part = e2e_crc8(check, 4U, 0x00U);
    TEST_ASSERT_EQUAL_HEX8(0x4B, e2e_crc8(&check[4], 5U, part));
}

static void test_e2e_sequence(void)
{
    uint8_t buf[8] = { 1, 2, 3, 4, 5, 0, 0xFF, 0 };
    uint8_t tx_counter = 0U;
    e2e_rx_state_t rx = { 0U, false };

    e2e_protect(0x100U, buf, 8U, 7U, 5U, &tx_counter);
    TEST_ASSERT_EQUAL(E2E_STATUS_INITIAL, e2e_check(0x100U, buf, 8U, 7U, 5U, &rx));
    TEST_ASSERT_EQUAL(E2E_STATUS_REPEATED, e2e_check(0x100U, buf, 8U, 7U, 5U, &rx));

    e2e_protect(0x100U, buf, 8U, 7U, 5U, &tx_counter);
    TEST_ASSERT_EQUAL(E2E_STATUS_OK, e2e_check(0x100U, buf, 8U, 7U, 5U, &rx));

    e2e_protect(0x100U, buf, 8U, 7U, 5U, &tx_counter);    /* lost on the bus */
    e2e_protect(0x100U, buf, 8U, 7U, 5U, &tx_counter);
    TEST_ASSERT_EQUAL(E2E_STATUS_OK_SOME_LOST, e2e_check(0x100U, buf, 8U, 7U, 5U, &rx));

    for (uint8_t i = 0U; i < 5U; i++)
    {
        e2e_protect(0x100U, buf, 8U, 7U, 5U, &tx_counter);
    }
    TEST_ASSERT_EQUAL(E2E_STATUS_WRONG_SEQUENCE, e2e_check(0x100U, buf, 8U, 7U, 5U, &rx));
}

static void test_e2e_counter_wraps(void)
{
    uint8_t buf[4] = { 0 };
    uint8_t tx_counter = 15U;
    e2e_rx_state_t rx = { 14U, true };

    e2e_protect(0x200U, buf, 4U, 3U, 2U, &tx_counter);
    TEST_ASSERT_EQUAL(E2E_STATUS_OK, e2e_check(0x200U, buf, 4U, 3U, 2U, &rx));
    TEST_ASSERT_EQUAL_UINT8(0U, tx_counter);
    e2e_protect(0x200U, buf, 4U, 3U, 2U, &tx_counter);
    TEST_ASSERT_EQUAL(E2E_STATUS_OK, e2e_check(0x200U, buf, 4U, 3U, 2U, &rx));
}

static void test_e2e_detects_bit_flip_and_wrong_id(void)
{
    uint8_t buf[8] = { 0x10, 0x20, 0x30, 0x40, 0x50, 0x00, 0xFF, 0x00 };
    uint8_t c = 3U;
    e2e_rx_state_t rx = { 0U, false };

    e2e_protect(0x100U, buf, 8U, 7U, 5U, &c);
    buf[2] ^= 0x04U;
    TEST_ASSERT_EQUAL(E2E_STATUS_CRC_ERROR, e2e_check(0x100U, buf, 8U, 7U, 5U, &rx));
    buf[2] ^= 0x04U;
    TEST_ASSERT_EQUAL(E2E_STATUS_CRC_ERROR, e2e_check(0x101U, buf, 8U, 7U, 5U, &rx)); /* masquerade */
    TEST_ASSERT_FALSE(rx.synced);
}

static void test_engine_data_matches_dbc(void)
{
    for (size_t i = 0U; i < ARRAY_LEN(ENGINE_VECTORS); i++)
    {
        const engine_vec_t *v = &ENGINE_VECTORS[i];
        msg_engine_data_t m;
        uint8_t frame[8];

        can_unpack_engine_data(v->frame, &m);
        TEST_ASSERT_EQUAL_UINT16(v->rpm_raw, m.engine_speed_raw);
        TEST_ASSERT_EQUAL_UINT16(v->speed_raw, m.vehicle_speed_raw);
        TEST_ASSERT_EQUAL_UINT8(v->coolant_raw, m.coolant_temp_raw);
        TEST_ASSERT_EQUAL_UINT8(v->alive, m.alive);
        TEST_ASSERT_EQUAL_UINT8(v->crc, m.crc);

        can_pack_engine_data(&m, frame);
        TEST_ASSERT_EQUAL_HEX8_ARRAY(v->frame, frame, 8);
        TEST_ASSERT_EQUAL_HEX8(v->crc, e2e_compute(CANID_ENGINE_DATA, frame, 8U, ENGINE_DATA_CRC_POS));
    }
}

static void test_body_status_matches_dbc(void)
{
    for (size_t i = 0U; i < ARRAY_LEN(BODY_VECTORS); i++)
    {
        const body_vec_t *v = &BODY_VECTORS[i];
        msg_body_status_t m;
        uint8_t frame[4];

        can_unpack_body_status(v->frame, &m);
        TEST_ASSERT_EQUAL(((v->bits >> 0) & 1U) != 0U, m.turn_left);
        TEST_ASSERT_EQUAL(((v->bits >> 1) & 1U) != 0U, m.turn_right);
        TEST_ASSERT_EQUAL(((v->bits >> 2) & 1U) != 0U, m.high_beam);
        TEST_ASSERT_EQUAL(((v->bits >> 3) & 1U) != 0U, m.door_open);
        TEST_ASSERT_EQUAL(((v->bits >> 4) & 1U) != 0U, m.seatbelt_unfastened);
        TEST_ASSERT_EQUAL_UINT8(v->alive, m.alive);

        can_pack_body_status(&m, frame);
        TEST_ASSERT_EQUAL_HEX8_ARRAY(v->frame, frame, 4);
        TEST_ASSERT_EQUAL_HEX8(v->crc, e2e_compute(CANID_BODY_STATUS, frame, 4U, BODY_STATUS_CRC_POS));
    }
}

static void test_cluster_status_matches_dbc(void)
{
    for (size_t i = 0U; i < ARRAY_LEN(CLUSTER_VECTORS); i++)
    {
        const cluster_vec_t *v = &CLUSTER_VECTORS[i];
        msg_cluster_status_t m;
        uint8_t frame[8];

        can_unpack_cluster_status(v->frame, &m);
        TEST_ASSERT_EQUAL_UINT8(v->fuel, m.fuel_level_pct);
        TEST_ASSERT_EQUAL_UINT32(v->odo, m.odometer_raw);
        TEST_ASSERT_EQUAL_UINT8(v->warn, m.warning_bits);
        TEST_ASSERT_EQUAL_UINT8(v->dtc, m.dtc_count);
        TEST_ASSERT_EQUAL_UINT8(v->alive, m.alive);

        can_pack_cluster_status(&m, frame);
        TEST_ASSERT_EQUAL_HEX8_ARRAY(v->frame, frame, 8);
    }
}

static void test_bits_unaligned_roundtrip(void)
{
    uint8_t buf[8];
    (void)memset(buf, 0xA5, sizeof(buf));
    can_put_bits_le(buf, 13U, 11U, 0x5A3U);
    TEST_ASSERT_EQUAL_HEX32(0x5A3U, can_get_bits_le(buf, 13U, 11U));
    TEST_ASSERT_EQUAL_HEX8(0xA5, buf[0]);                 /* neighbours untouched */
    TEST_ASSERT_EQUAL_HEX8(0x05, buf[1] & 0x1FU);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_crc8_j1850_check_value);
    RUN_TEST(test_crc8_chaining_equals_single_pass);
    RUN_TEST(test_e2e_sequence);
    RUN_TEST(test_e2e_counter_wraps);
    RUN_TEST(test_e2e_detects_bit_flip_and_wrong_id);
    RUN_TEST(test_engine_data_matches_dbc);
    RUN_TEST(test_body_status_matches_dbc);
    RUN_TEST(test_cluster_status_matches_dbc);
    RUN_TEST(test_bits_unaligned_roundtrip);
    return UNITY_END();
}
