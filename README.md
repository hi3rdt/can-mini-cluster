# CAN Mini Instrument Cluster

A two-node CAN network on hobby hardware:

- **Cluster ECU**: STM32F103C8T6 (Blue Pill), **bare-metal C at register level, no HAL/LL**. It receives vehicle data over CAN at 500 kbit/s, checks every frame, and draws a dashboard on a 320x240 ILI9341 TFT.
- **Vehicle simulator**: ESP32-WROOM (ESP-IDF / FreeRTOS). It sends engine and body messages, reads back the cluster status, and can inject faults from a serial console.

```
 ILI9341 TFT ── SPI1 ───┐                          ┌── UART console (laptop)
 Fuel pot ──── ADC1 ────┤   STM32F103 cluster      │   ESP32 vehicle simulator
 Debug log ─── USART1 ──┤   (bare-metal)           │   (ESP-IDF, FreeRTOS)
                        │                          │
                  bxCAN PB8/PB9               TWAI GPIO5/GPIO4
                        │                          │
                  [transceiver]              [transceiver]
                        │                          │
      120 Ω ════════════╧════ CAN 500 kbit/s ══════╧════════════ 120 Ω
```

## Features

| Area | What is implemented |
| --- | --- |
| Bare-metal platform | C startup code and vector table, linker script with a stack-size check, 72 MHz clock tree (HSE + PLL) with start-up timeout, reset-cause readout |
| Drivers | RCC, GPIO, SysTick, USART (interrupt-driven ring buffers), ADC, SPI + DMA, bxCAN (hardware ID filter, RX interrupt into a queue, error counters, bus-off auto recovery), IWDG |
| CAN communication | Message layout defined in a DBC file (`docs/can_matrix.dbc`), signal packing shared by both nodes, **E2E protection** (CRC8 SAE J1850 + 4-bit alive counter), **timeout monitoring** (5 missed cycles = signal lost) |
| Application | Speed, rpm, coolant, fuel (low-pass filtered), odometer, turn/high-beam/door/belt telltales, warnings with hysteresis, fail-safe `---` display when a message is lost |
| Display | Partial redraw (each widget redraws only when its value changes), no framebuffer |
| Quality | 17 host unit tests (Unity, AddressSanitizer + UBSan), DBC cross-check with cantools-generated vectors, cppcheck, `-Wall -Wextra -Werror`, GitHub Actions CI |

Measured from the build: **9.4 KB Flash (15 %), 4.8 KB RAM (24 %)** on the STM32.

## CAN matrix

| ID | Name | Sender | Cycle | Content |
| --- | --- | --- | --- | --- |
| 0x100 | EngineData | ESP32 | 20 ms | rpm, vehicle speed, coolant, alive counter, CRC |
| 0x200 | BodyStatus | ESP32 | 100 ms | turn left/right, high beam, door, seatbelt, alive counter, CRC |
| 0x300 | ClusterStatus | STM32 | 100 ms | fuel level, odometer, warning bits, alive counter, CRC |

## Repository layout

```
common/              shared by STM32, ESP32 and the tests: can_matrix, e2e
stm32_cluster/
  startup/ linker/   C startup + vector table, linker script
  cmsis/             ARM CMSIS core + ST device header (register definitions only)
  config/board.h     pin map, clocks, IRQ priorities
  mcal/              register-level drivers
  ecual/             ILI9341 display driver
  services/          com (RX dispatch, E2E, timeouts), log (printf to UART)
  gfx/               fill/text/bar primitives + 8x8 font
  app/               cluster logic, dashboard, main loop
esp32_vehicle_sim/   ESP-IDF project: TWAI driver, vehicle model, console
test/                host unit tests (Unity), fake bxCAN, generated DBC vectors
tools/               gen_test_vectors.py (cantools)
docs/                architecture notes, CAN matrix (.dbc)
```

## Wiring

| Signal | STM32 Blue Pill | ESP32 | Note |
| --- | --- | --- | --- |
| CAN TX / RX | PB9 / PB8 | GPIO5 / GPIO4 | each to its own transceiver TXD/RXD |
| CANH / CANL | transceiver | transceiver | twisted pair, 120 Ω at **both** ends (about 60 Ω measured between CANH and CANL, power off) |
| GND | GND | GND | common ground is mandatory |
| TFT SCK / MOSI / MISO | PA5 / PA7 / PA6 | | SPI1 |
| TFT CS / DC / RST / LED | PA4 / PB0 / PB1 / PA8 | | |
| Debug UART | PA9 TX / PA10 RX | USB | 115200 8N1 on both |
| Fuel level | PA0 (10k pot between 3V3 and GND) | | |

The ESP32 is not 5 V tolerant. With a 5 V transceiver (TJA1050, MCP2551), put a 1k/2k divider on the ESP32 RX line, or use SN65HVD230 (3.3 V) transceivers.

## Build and flash

**STM32** needs `arm-none-eabi-gcc` and OpenOCD with an ST-Link V2.

```sh
make -C stm32_cluster            # build/cluster.elf, .bin, .hex
make -C stm32_cluster flash      # OpenOCD + ST-Link
make -C stm32_cluster check      # cppcheck
```

**ESP32** needs ESP-IDF 5.x (tested with 5.3.1).

```sh
cd esp32_vehicle_sim
idf.py set-target esp32
idf.py build flash monitor
```

**Host unit tests** need gcc.

```sh
make -C test                     # 17 tests, AddressSanitizer + UBSan
make -C test vectors             # regenerate DBC vectors (pip install cantools)
```

## Demo

1. Power both boards. The ESP32 starts in `sweep` mode: speed runs 0 to 140 to 0 km/h, rpm follows, and the indicators, high beam and seatbelt cycle through.
2. ESP32 console: `speed 130`. The speed turns red (overspeed warning).
3. `mute engine`. After 100 ms (5 missed cycles) the cluster shows `---` and `ENGINE LOST`. `mute none` recovers it.
4. `crc on`. The ESP32 keeps sending, but every EngineData frame fails the CRC check, so the cluster still reports the signal as lost. `crc off` recovers it.
5. `log on` on the ESP32 prints every frame in `candump` format. The STM32 prints a status line with counters once per second.
6. Disconnect CANH: the error counters (TEC/REC) rise on the dashboard. Reconnect it and the bus recovers on its own.

## Status and limitations

- Both firmwares compile cleanly and the host unit tests pass. **The firmware has not been run on real hardware yet**, so expect a bring-up session. The ILI9341 `MADCTL` orientation differs between panels (see `ecual/ili9341.c`).
- The watchdog is refreshed once per main-loop pass. It catches a hang, but not one part of the loop being starved while the rest keeps running.
- SPI DMA transfers wait for completion, so a large redraw delays the loop by its SPI time.
- The odometer is kept in RAM only.

The `full` branch extends this project with UDS diagnostics over ISO-TP, DTC memory in Flash, a task scheduler with execution-time measurement and an alive-supervised watchdog.

## Third-party code

- ARM CMSIS Core and the ST `cmsis_device_f1` headers (Apache-2.0), in `stm32_cluster/cmsis`
- Unity test framework (MIT), in `test/unity`
- `font8x8_basic` by Daniel Hepper (Public Domain), in `stm32_cluster/gfx/font8x8.c`
