# Architecture

## Layers (STM32 cluster)

The firmware follows a scaled-down AUTOSAR Classic layering. Each layer only calls the layer directly below it, and the hardware is only touched in `mcal/`.

| Layer | Directory | AUTOSAR analogue | Content |
| --- | --- | --- | --- |
| Application | `app/` | SWC | `cluster` (signal selection, plausibility, warnings, odometer), `dashboard`, `cli`, `app_info` (DIDs) |
| Services | `services/` | BSW services | `sched` (OS), `com` (Com + E2E), `diag` + `uds` (Dcm), `dem` (Dem), `nvm` (NvM/Fee), `wdg_m` (WdgM), `log` |
| Transport | `common/` | CanTp, Com signal layer | `isotp`, `can_matrix`, `e2e` (shared with the ESP32) |
| ECU abstraction | `ecual/`, `gfx/` | ECU abstraction | `ili9341`, `buttons`, `speed_sensor`, `gfx` |
| MCAL | `mcal/` | MCAL | `rcc`, `gpio`, `systick`, `uart`, `exti`, `tim`, `adc`, `spi`, `bxcan`, `iwdg`, `flash`, `fault` |

## Task model

ISRs only move data into queues or latch flags. Everything else runs in four cooperative, time-triggered tasks, so there is no shared-data race between tasks (all run in thread mode, to completion).

| Task | Period | Offset | Work | Watchdog bit |
| --- | --- | --- | --- | --- |
| `1ms` | 1 ms | 0 | COM RX dispatch (max 8 frames), ISO-TP/UDS timers, CLI | 0 |
| `10ms` | 10 ms | 1 | deadline monitoring, E2E/bus-off DEM reporting, buttons | 1 |
| `20ms` | 20 ms | 3 | cluster logic, dashboard partial redraw | 2 |
| `100ms` | 100 ms | 7 | ClusterStatus TX, DEM to NVM, heartbeat LED, watchdog supervision | 3 |

Offsets keep the tasks from being released in the same tick. `stats` on the CLI prints the measured last and worst-case execution time of each task (DWT `CYCCNT`), the CPU load of the last second and the overrun count. The load figure covers task time only; ISR time is not included.

## Interrupts

| IRQ | Priority | Work |
| --- | --- | --- |
| TIM2 | 3 | wheel-pulse capture, 32-bit extension of the 16-bit counter |
| CAN RX0 | 5 | FIFO0 into 32-frame SPSC queue |
| CAN SCE | 6 | bus-off event latch |
| USART1 | 7 | TX/RX ring buffers |
| EXTI15_10 | 8 | button edges with timestamp debounce |
| SysTick | 10 | 1 ms tick |

Priority grouping uses 4 bits of pre-emption and no sub-priority.

## Data flow of one speed value

1. The ESP32 packs `VehicleSpeed` into 0x100, adds the alive counter and CRC8, and sends it every 20 ms.
2. bxCAN filter bank 0 (ID list) accepts the frame. The RX0 ISR copies it into the queue.
3. `com_main_1ms` dequeues it and runs `e2e_check`. Only a frame with a valid CRC and a plausible counter restarts the 100 ms deadline and updates the signal buffer.
4. `cluster_main_20ms` picks the speed source (CAN, else wheel pulse, else none), runs the 10 km/h / 2 s plausibility check against the wheel pulse, and updates the odometer and warnings.
5. `dashboard_update` redraws the speed digits only if the text or colour changed.
6. `cluster_main_100ms` reports the same snapshot in ClusterStatus 0x300.

## Memory budget

| Region | Used | Available | Note |
| --- | --- | --- | --- |
| Flash | 18.1 KB | 63 KB | last 1 KB page reserved for NVM |
| RAM (static) | 5.7 KB | 20 KB | includes the 2 KB minimum stack reservation |
| Stack | measured at run time | rest of RAM | `stats` prints the painted high-water mark |

Largest static users: UART TX ring 1 KB, line buffer 640 B, CAN RX queue 32 x 16 B, ISO-TP buffers 3 x 128 B.

## Design decisions

- **No framebuffer.** 320 x 240 x 2 B = 150 KB does not fit in 20 KB of RAM. Widgets cache what they last drew and repaint only on change, and text is streamed one scan line at a time.
- **16-bit SPI frames for pixels.** The ILI9341 expects RGB565 MSB first, and a 16-bit SPI frame sends exactly that, so no byte swap is needed.
- **Cooperative scheduler instead of an RTOS.** All timing is deterministic and measurable. The cost is that a long task delays the others, which the WCET table makes visible.
- **The deadline restarts only on E2E-valid frames.** A sender that keeps transmitting corrupt data is treated like a lost sender.
- **Watchdog with alive supervision.** Refreshing the IWDG in the idle loop would not detect a single starving task. Here every task must check in during each 100 ms window.
- **NVM magic is written last.** A reset in the middle of a record write leaves a slot that fails validation and is skipped. The newest valid slot wins. This is covered by the unit test `test_nvm_interrupted_write_keeps_previous`.
- **Shared code between nodes.** `can_matrix`, `e2e` and `isotp` compile unchanged for Cortex-M3, Xtensa and the host, so sender, receiver and tests cannot drift apart.
