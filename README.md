# CAN Mini Instrument Cluster

A two-node automotive CAN network built on hobby hardware:

- **Cluster ECU**: STM32F103C8T6 (Blue Pill), **bare-metal C, register level, no HAL/LL**. It receives vehicle data over CAN, draws a 320x240 dashboard on an ILI9341 TFT, and acts as a **UDS diagnostic server** with DTC memory in Flash.
- **Vehicle simulator + tester**: ESP32-WROOM (ESP-IDF / FreeRTOS). It sends engine and body messages, produces a wheel-speed pulse, injects faults, and runs a **UDS client** over ISO-TP.

```
 ILI9341 TFT ── SPI1+DMA ──┐                         ┌── UART console (laptop)
 Fuel pot ──── ADC1+DMA ───┤                         │
 Buttons ───── EXTI ───────┤   STM32F103 cluster     │   ESP32 vehicle sim
 Buzzer ────── TIM4 PWM ───┤   (bare-metal)          │   + UDS tester
 Wheel pulse ─ TIM2 IC <───┼──────── 3.3 V pulse ────┤   (ESP-IDF)
                           │                         │
                     bxCAN PB8/PB9              TWAI GPIO5/GPIO4
                           │                         │
                     [transceiver]             [transceiver]
                           │                         │
        120 Ω ═════════════╧═══ CAN 500 kbit/s ══════╧═════════════ 120 Ω
```

## Features

| Area | What is implemented |
| --- | --- |
| Bare-metal platform | Own C startup code and vector table, linker script with a reserved NVM page, 72 MHz clock tree with HSE start-up timeout and clock security system (CSS), stack painting for the high-water mark, HardFault handler with CFSR/HFSR/BFAR dump |
| Drivers (MCAL) | RCC, GPIO, SysTick, EXTI, USART (IRQ ring buffers), TIM (PWM, 32-bit extended input capture), ADC scan + circular DMA, SPI + DMA, bxCAN (ID-list filter, RX queue, error/bus-off ISR), IWDG, Flash |
| CAN network | DBC-defined matrix (`docs/can_matrix.dbc`), Intel signal packing shared by both nodes, **E2E protection** (CRC8 SAE J1850 + 4-bit alive counter, Data ID in the CRC), deadline monitoring (5x cycle), bus-off auto recovery |
| Diagnostics | **ISO-TP** (ISO 15765-2: SF/FF/CF/FC, BS, STmin, N_Bs/N_Cr timeouts) and **UDS** (ISO 14229-1: 0x10, 0x11, 0x14, 0x19 01/02, 0x22, 0x3E, NRCs, S3 timeout, functional addressing) |
| DTC memory | DEM with ISO 14229 status bits, occurrence counter, 6 DTCs (U0100, U0140, U0001, U0401, B1000, B1002), power-fail-safe append-only NVM in Flash |
| Safety mechanisms | Alive-supervised watchdog (all tasks must check in), reset-cause logging (watchdog reset leads to a DTC), CAN speed vs. wheel-pulse plausibility check, graceful degradation to wheel speed when CAN is lost |
| Real-time | Time-triggered cooperative scheduler (1/10/20/100 ms), WCET per task via DWT cycle counter, CPU load, overrun detection, `WFI` in idle |
| HMI | Dashboard with dirty-checked partial redraw, telltales, blinking indicators, warnings with hysteresis, overspeed/seatbelt chime |
| Quality | 25 host unit tests (Unity, ASan + UBSan), DBC to C cross-check with cantools test vectors, MISRA C:2012 check with cppcheck, `-Wall -Wextra -Werror`, GitHub Actions CI |

Measured from the build: **18.1 KB Flash (28 %), 5.7 KB RAM (28 %)** on the STM32.

## Repository layout

```
common/              shared by STM32 and ESP32: can_matrix, e2e, isotp
stm32_cluster/
  startup/ linker/   C startup + vector table, linker script
  cmsis/             ARM CMSIS core + ST device header only (register definitions)
  config/board.h     pin map, clocks, IRQ priorities
  mcal/              register-level drivers
  ecual/             ILI9341, buttons, speed sensor
  services/          sched, com, diag, uds, dem, nvm, wdg_m, log
  gfx/               primitives + 8x8 font
  app/               cluster logic, dashboard, CLI, DID provider, main
esp32_vehicle_sim/   ESP-IDF project: TWAI, simulator, UDS tester, console
test/                host unit tests (Unity) + generated DBC vectors
tools/               gen_test_vectors.py (cantools)
docs/                architecture, CAN matrix (.dbc), UDS spec, MISRA deviations
```

## Wiring

| Signal | STM32 Blue Pill | ESP32 | Note |
| --- | --- | --- | --- |
| CAN TX / RX | PB9 / PB8 (bxCAN remap 2) | GPIO5 / GPIO4 | each to its own transceiver TXD/RXD |
| CANH / CANL | transceiver | transceiver | twisted pair, 120 Ω at **both** ends (60 Ω measured across CANH-CANL) |
| Wheel pulse | PA1 (TIM2_CH2) | GPIO18 (LEDC) | 3.3 V logic, direct wire |
| GND | GND | GND | common ground is mandatory |
| TFT SCK / MOSI / MISO | PA5 / PA7 / PA6 | | SPI1, 36 MHz |
| TFT CS / DC / RST / LED | PA4 / PB0 / PB1 / PA8 | | LED = TIM1 PWM backlight |
| Debug UART | PA9 TX / PA10 RX | USB | 115200 8N1 on both |
| Fuel level | PA0 (pot 10k, 3V3-GND) | | |
| Buttons | PB12 MENU / PB13 TRIP to GND | | internal pull-ups |
| Buzzer | PB6 (TIM4_CH1) | | passive piezo |

The ESP32 is not 5 V tolerant. With a 5 V transceiver (TJA1050, MCP2551), put a 1k/2k divider on the ESP32 RX line, or use SN65HVD230 (3.3 V) transceivers. The STM32 PB8 pin is 5 V tolerant.

## Build and flash

**STM32** needs `arm-none-eabi-gcc` and OpenOCD with an ST-Link V2.

```sh
make -C stm32_cluster            # build/cluster.elf, .bin, .hex
make -C stm32_cluster flash      # OpenOCD + ST-Link
make -C stm32_cluster misra      # cppcheck MISRA report in build/misra.txt
```

**ESP32** needs ESP-IDF 5.x (tested with 5.3.1).

```sh
cd esp32_vehicle_sim
idf.py set-target esp32
idf.py build flash monitor
```

**Host unit tests** need gcc.

```sh
make -C test                     # 25 tests, AddressSanitizer + UBSan
make -C test vectors             # regenerate DBC vectors (pip install cantools)
```

## Demo script

1. Power both boards. The ESP32 starts in `sweep` mode: speed runs 0 to 140 to 0 km/h, rpm follows, and the indicators, high beam and seatbelt cycle through.
2. ESP32 console: `mode manual`, `speed 130`. The speed turns red and the buzzer beeps (overspeed).
3. `mute engine`. After about 100 ms (5 missed cycles) the cluster shows `---` for rpm and temperature, switches the speed to the wheel pulse (`WHEEL` tag), and DTC U0100 is set. Run `mute none` to recover.
4. `crc on`. The cluster rejects every EngineData frame, so U0401 and U0100 are set. `crc off` to recover.
5. `pulse_offset 25`. After 2 s the speed plausibility DTC B1002 is set.
6. `dtc` reads the DTCs over UDS, `vin` reads DID F190, `ext` then `clear` clears them, and `reset` resets the ECU.
7. STM32 console: `stats` shows WCET per task, CPU load and stack usage. `wdgtest` provokes a watchdog reset, which is logged as DTC B1000. `fault` provokes a UsageFault and prints the register dump.
8. Unplug CANH to provoke error frames. `can` on the STM32 shows TEC/REC and bus-off events, and the bus recovers on its own when you plug it back in.

## Status and limitations

- Both firmwares compile cleanly. The host unit tests pass. **The firmware has not yet been run on real hardware**, so expect a bring-up session. The ILI9341 `MADCTL` orientation in particular differs between panels (see `ecual/ili9341.c`).
- The SPI DMA transfers wait for completion, so the 20 ms task WCET includes the SPI time of the widgets that changed. A completion-interrupt pipeline is the next optimisation.
- A Flash page erase (20-40 ms) stalls the CPU on the single-bank F103. It happens at most once every 16 DTC updates and is accounted for in the watchdog timeout.
- MISRA findings that remain are listed with rationale in `docs/misra_deviations.md`.
- The odometer is kept in RAM. Persisting it via NVM is a planned extension.

## Third-party code

- ARM CMSIS Core and the ST `cmsis_device_f1` headers (Apache-2.0), in `stm32_cluster/cmsis`
- Unity test framework (MIT), in `test/unity`
- `font8x8_basic` by Daniel Hepper (Public Domain), in `stm32_cluster/gfx/font8x8.c`
