/**
 * @file    test_diag.c
 * @brief   Host tests: NVM record store, DEM status handling, UDS server services and NRCs.
 */
#include "unity.h"
#include "nvm.h"
#include "dem.h"
#include "uds.h"
#include "fake_flash.h"
#include <string.h>

static uint8_t  s_resp[256];
static uint32_t s_resets;
static uint32_t s_now;

static uint16_t read_did(uint16_t did, uint8_t *out, uint16_t max)
{
    static const char vin[] = "VNDEMOCLUSTER0001";
    if ((did == 0xF190U) && (max >= 17U)) { (void)memcpy(out, vin, 17U); return 17U; }
    if ((did == 0x0100U) && (max >= 2U))  { out[0] = 0x2E; out[1] = 0xE0; return 2U; }
    return 0U;
}

static void hard_reset(void) { s_resets++; }

static const uds_cfg_t s_uds_cfg = { read_did, hard_reset };

static uint16_t req(const uint8_t *r, uint16_t len, bool functional)
{
    return uds_process(r, len, functional, s_resp, sizeof(s_resp), s_now);
}

void setUp(void)
{
    fake_flash_reset();
    (void)nvm_init();
    dem_init();
    uds_init(&s_uds_cfg);
    s_resets = 0U;
    s_now = 10000U;
}
void tearDown(void) {}

/* ---------------- NVM ---------------- */
static void test_nvm_empty_then_roundtrip(void)
{
    uint8_t a[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, b[10];
    TEST_ASSERT_EQUAL(E_NOT_OK, nvm_read(b, sizeof(b)));
    TEST_ASSERT_EQUAL(E_OK, nvm_write(a, sizeof(a)));
    (void)nvm_init();                                     /* "power cycle" */
    TEST_ASSERT_EQUAL(E_OK, nvm_read(b, sizeof(b)));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(a, b, sizeof(a));
}

static void test_nvm_wraps_page_and_keeps_newest(void)
{
    uint8_t v[12], r[12];
    for (uint8_t i = 0U; i < 40U; i++)                    /* 16 slots per page */
    {
        (void)memset(v, i, sizeof(v));
        TEST_ASSERT_EQUAL(E_OK, nvm_write(v, sizeof(v)));
    }
    TEST_ASSERT_EQUAL_UINT32(2U, g_fake_erase_count);
    (void)nvm_init();
    TEST_ASSERT_EQUAL(E_OK, nvm_read(r, sizeof(r)));
    TEST_ASSERT_EACH_EQUAL_UINT8(39U, r, sizeof(r));
}

static void test_nvm_interrupted_write_keeps_previous(void)
{
    uint8_t old[12], neu[12], r[12];
    (void)memset(old, 0x11, sizeof(old));
    (void)memset(neu, 0x22, sizeof(neu));
    TEST_ASSERT_EQUAL(E_OK, nvm_write(old, sizeof(old)));
    g_fake_fail_after = 5;                                /* reset in the middle of the body */
    TEST_ASSERT_EQUAL(E_NOT_OK, nvm_write(neu, sizeof(neu)));
    g_fake_fail_after = -1;
    (void)nvm_init();
    TEST_ASSERT_EQUAL(E_OK, nvm_read(r, sizeof(r)));
    TEST_ASSERT_EACH_EQUAL_UINT8(0x11U, r, sizeof(r));
    TEST_ASSERT_EQUAL(E_OK, nvm_write(neu, sizeof(neu))); /* half-written slot is skipped */
    (void)nvm_init();
    TEST_ASSERT_EQUAL(E_OK, nvm_read(r, sizeof(r)));
    TEST_ASSERT_EACH_EQUAL_UINT8(0x22U, r, sizeof(r));
}

/* ---------------- DEM ---------------- */
static void test_dem_confirm_persist_and_new_cycle(void)
{
    dem_report(DEM_EVT_ENGINE_TIMEOUT, true);
    TEST_ASSERT_EQUAL_HEX8(0x2F, dem_status(DEM_EVT_ENGINE_TIMEOUT));
    dem_report(DEM_EVT_ENGINE_TIMEOUT, false);
    TEST_ASSERT_EQUAL_HEX8(0x2E, dem_status(DEM_EVT_ENGINE_TIMEOUT));
    dem_report(DEM_EVT_ENGINE_TIMEOUT, true);
    TEST_ASSERT_EQUAL_UINT8(2U, dem_occurrences(DEM_EVT_ENGINE_TIMEOUT));
    dem_main_100ms();

    (void)nvm_init();                                     /* power cycle */
    dem_init();
    TEST_ASSERT_EQUAL_HEX8(0x28, dem_status(DEM_EVT_ENGINE_TIMEOUT)); /* CDTC + TFSLC kept */
    TEST_ASSERT_EQUAL_UINT8(2U, dem_occurrences(DEM_EVT_ENGINE_TIMEOUT));
    TEST_ASSERT_EQUAL_UINT8(1U, dem_confirmed_count());
}

static void test_dem_no_flash_write_when_nothing_changes(void)
{
    dem_report(DEM_EVT_CAN_BUSOFF, true);
    dem_main_100ms();
    const uint16_t writes = nvm_write_count();
    dem_report(DEM_EVT_CAN_BUSOFF, true);                 /* still failed: no new edge */
    dem_report(DEM_EVT_CAN_BUSOFF, false);
    dem_main_100ms();
    TEST_ASSERT_EQUAL_UINT16(writes, nvm_write_count());
}

/* ---------------- UDS ---------------- */
static void test_uds_session_control_and_s3_timeout(void)
{
    const uint8_t r[] = { 0x10, 0x03 };
    const uint8_t expect[] = { 0x50, 0x03, 0x00, 0x32, 0x01, 0xF4 };
    TEST_ASSERT_EQUAL_UINT16(6U, req(r, 2U, false));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expect, s_resp, 6);
    TEST_ASSERT_EQUAL_UINT8(UDS_SESSION_EXTENDED, uds_session());
    uds_main(s_now + UDS_S3_TIMEOUT_MS - 1U);
    TEST_ASSERT_EQUAL_UINT8(UDS_SESSION_EXTENDED, uds_session());
    uds_main(s_now + UDS_S3_TIMEOUT_MS);
    TEST_ASSERT_EQUAL_UINT8(UDS_SESSION_DEFAULT, uds_session());
}

static void test_uds_read_did_single_and_multiple(void)
{
    const uint8_t r1[] = { 0x22, 0xF1, 0x90 };
    TEST_ASSERT_EQUAL_UINT16(20U, req(r1, 3U, false));
    TEST_ASSERT_EQUAL_HEX8(0x62, s_resp[0]);
    TEST_ASSERT_EQUAL_MEMORY("VNDEMOCLUSTER0001", &s_resp[3], 17);

    const uint8_t r2[] = { 0x22, 0x01, 0x00, 0xF1, 0x90 };
    TEST_ASSERT_EQUAL_UINT16(1U + 4U + 19U, req(r2, 5U, false));
    TEST_ASSERT_EQUAL_HEX8(0x2E, s_resp[3]);

    const uint8_t r3[] = { 0x22, 0x12, 0x34 };
    TEST_ASSERT_EQUAL_UINT16(3U, req(r3, 3U, false));
    const uint8_t nrc[] = { 0x7F, 0x22, 0x31 };
    TEST_ASSERT_EQUAL_HEX8_ARRAY(nrc, s_resp, 3);

    const uint8_t r4[] = { 0x22, 0xF1 };
    TEST_ASSERT_EQUAL_UINT16(3U, req(r4, 2U, false));
    TEST_ASSERT_EQUAL_HEX8(0x13, s_resp[2]);
}

static void test_uds_read_and_clear_dtc(void)
{
    dem_report(DEM_EVT_BODY_TIMEOUT, true);
    dem_report(DEM_EVT_WDG_RESET, true);

    const uint8_t cnt[] = { 0x19, 0x01, 0x08 };
    const uint8_t cnt_rsp[] = { 0x59, 0x01, 0x2F, 0x01, 0x00, 0x02 };
    TEST_ASSERT_EQUAL_UINT16(6U, req(cnt, 3U, false));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(cnt_rsp, s_resp, 6);

    const uint8_t lst[] = { 0x19, 0x02, 0x08 };
    const uint8_t lst_rsp[] = { 0x59, 0x02, 0x2F, 0xC1, 0x40, 0x00, 0x2F, 0x90, 0x00, 0x00, 0x2F };
    TEST_ASSERT_EQUAL_UINT16(11U, req(lst, 3U, false));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(lst_rsp, s_resp, 11);

    const uint8_t clr[] = { 0x14, 0xFF, 0xFF, 0xFF };
    TEST_ASSERT_EQUAL_UINT16(3U, req(clr, 4U, false));        /* default session */
    TEST_ASSERT_EQUAL_HEX8(0x7F, s_resp[2]);

    const uint8_t ext[] = { 0x10, 0x03 };
    (void)req(ext, 2U, false);
    TEST_ASSERT_EQUAL_UINT16(1U, req(clr, 4U, false));
    TEST_ASSERT_EQUAL_HEX8(0x54, s_resp[0]);
    TEST_ASSERT_EQUAL_UINT8(0U, dem_confirmed_count());
}

static void test_uds_ecu_reset_after_delay(void)
{
    const uint8_t r[] = { 0x11, 0x01 };
    TEST_ASSERT_EQUAL_UINT16(2U, req(r, 2U, false));
    uds_main(s_now + UDS_RESET_DELAY_MS - 1U);
    TEST_ASSERT_EQUAL_UINT32(0U, s_resets);
    uds_main(s_now + UDS_RESET_DELAY_MS);
    TEST_ASSERT_EQUAL_UINT32(1U, s_resets);
}

static void test_uds_tester_present_suppress_and_functional_nrc(void)
{
    const uint8_t tp[] = { 0x3E, 0x80 };
    TEST_ASSERT_EQUAL_UINT16(0U, req(tp, 2U, true));          /* suppressPosRsp */
    const uint8_t tp0[] = { 0x3E, 0x00 };
    TEST_ASSERT_EQUAL_UINT16(2U, req(tp0, 2U, true));

    const uint8_t unknown[] = { 0x31, 0x01, 0x02, 0x03 };
    TEST_ASSERT_EQUAL_UINT16(0U, req(unknown, 4U, true));     /* NRC 0x11 suppressed */
    TEST_ASSERT_EQUAL_UINT16(3U, req(unknown, 4U, false));
    TEST_ASSERT_EQUAL_HEX8(0x11, s_resp[2]);

    const uint8_t badsub[] = { 0x10, 0x05 };
    TEST_ASSERT_EQUAL_UINT16(3U, req(badsub, 2U, false));
    TEST_ASSERT_EQUAL_HEX8(0x12, s_resp[2]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_nvm_empty_then_roundtrip);
    RUN_TEST(test_nvm_wraps_page_and_keeps_newest);
    RUN_TEST(test_nvm_interrupted_write_keeps_previous);
    RUN_TEST(test_dem_confirm_persist_and_new_cycle);
    RUN_TEST(test_dem_no_flash_write_when_nothing_changes);
    RUN_TEST(test_uds_session_control_and_s3_timeout);
    RUN_TEST(test_uds_read_did_single_and_multiple);
    RUN_TEST(test_uds_read_and_clear_dtc);
    RUN_TEST(test_uds_ecu_reset_after_delay);
    RUN_TEST(test_uds_tester_present_suppress_and_functional_nrc);
    return UNITY_END();
}
