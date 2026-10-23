# Diagnostic specification (cluster ECU)

Addressing: physical request 0x7E0, response 0x7E8, functional request 0x7DF (single frame only). ISO-TP padding is 0xCC. The server announces BS = 0 and STmin = 1 ms.

## Services

| SID | Service | Sub-functions / parameters | Session | Notes |
| --- | --- | --- | --- | --- |
| 0x10 | DiagnosticSessionControl | 0x01 default, 0x03 extended | any | response carries P2 = 50 ms and P2* = 5000 ms |
| 0x11 | ECUReset | 0x01 hardReset | any | reset 50 ms after the positive response |
| 0x14 | ClearDiagnosticInformation | group 0xFFFFFF | extended | NRC 0x7F in the default session |
| 0x19 | ReadDTCInformation | 0x01 count by mask, 0x02 DTCs by mask | any | availability mask 0x2F |
| 0x22 | ReadDataByIdentifier | 1 to 4 DIDs per request | any | NRC 0x31 if none of the DIDs is supported |
| 0x3E | TesterPresent | 0x00, 0x80 (suppress response) | any | |

The extended session falls back to default after 5 s without a request (S3). NRCs 0x11, 0x12 and 0x31 are suppressed for functional requests (ISO 14229-1 7.5).

## Data identifiers

| DID | Name | Format |
| --- | --- | --- |
| F190 | VIN | 17 ASCII |
| F195 | Software version | ASCII, for example `1.0.0` |
| F18C | ECU serial number | 12 bytes, STM32 96-bit unique ID |
| 0100 | Displayed vehicle speed | u16, km/h |
| 0101 | Fuel level | u8, % |
| 0102 | CPU load | u8 current %, u8 peak % |
| 0103 | Reset cause | u8: 0 power-on, 1 pin, 2 software, 3 IWDG, 4 WWDG, 5 low-power |
| 0104 | Odometer | u32, 0.1 km |
| 0105 | MCU temperature | s16, 0.1 degC |
| 0106 | CAN error state | u8 TEC, u8 REC, u32 bus-off count |
| 0107 | Uptime | u32, s |

## DTCs

| DTC | 3-byte code | Set when | Cleared (testFailed) when |
| --- | --- | --- | --- |
| U0100 | C1 00 00 | EngineData 0x100 not received with valid E2E for 100 ms | valid frames again |
| U0140 | C1 40 00 | BodyStatus 0x200 not received with valid E2E for 500 ms | valid frames again |
| U0001 | C0 01 00 | bxCAN entered bus-off | engine or body traffic received again |
| U0401 | C4 01 00 | 3 consecutive EngineData frames with a CRC or sequence error | a valid frame |
| B1000 | 90 00 00 | ECU started after an IWDG reset | immediately (event-type DTC, stays confirmed) |
| B1002 | 90 02 00 | CAN speed and wheel-pulse speed differ by more than 10 km/h for 2 s | 2 s of agreement |

Status bits: bit0 testFailed, bit1 testFailedThisOperationCycle, bit2 pendingDTC, bit3 confirmedDTC, bit5 testFailedSinceLastClear. Bits 3 and 5 and the occurrence counter are stored in Flash.

## Example session (ESP32 console)

```
> ext
UDS -> 10 03
UDS <- 50 03 00 32 01 F4
> dtc
UDS -> 19 02 FF
UDS <- 59 02 2F C1 00 00 2E
       1 DTC(s), availability mask 0x2F
       U0100-00  status 0x2E confirmed
> clear
UDS -> 14 FF FF FF
UDS <- 54
```
