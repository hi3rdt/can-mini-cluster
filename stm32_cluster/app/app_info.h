/**
 * @file    app_info.h
 * @brief   ECU identification, UDS DID provider and runtime figures (stack, reset cause).
 */
#ifndef APP_INFO_H
#define APP_INFO_H

#include "std_types.h"
#include "rcc.h"

/* Supported DIDs (ReadDataByIdentifier 0x22) */
#define DID_VIN             (0xF190U)   /* 17 ASCII                         */
#define DID_SW_VERSION      (0xF195U)   /* ASCII                            */
#define DID_ECU_SERIAL      (0xF18CU)   /* 12 bytes: STM32 96-bit unique ID */
#define DID_VEHICLE_SPEED   (0x0100U)   /* u16 km/h, as displayed           */
#define DID_FUEL_LEVEL      (0x0101U)   /* u8 %                             */
#define DID_CPU_LOAD        (0x0102U)   /* u8 current %, u8 peak %          */
#define DID_RESET_CAUSE     (0x0103U)   /* u8 reset_cause_t                 */
#define DID_ODOMETER        (0x0104U)   /* u32 0.1 km                       */
#define DID_MCU_TEMP        (0x0105U)   /* s16 0.1 degC                     */
#define DID_CAN_ERRORS      (0x0106U)   /* u8 TEC, u8 REC, u32 bus-off count */
#define DID_UPTIME          (0x0107U)   /* u32 s                            */

void     app_info_init(reset_cause_t rc);
uint16_t app_read_did(uint16_t did, uint8_t *out, uint16_t max);
uint32_t app_stack_used_bytes(void);
uint32_t app_stack_total_bytes(void);

#endif /* APP_INFO_H */
