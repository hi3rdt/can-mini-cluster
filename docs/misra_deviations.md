# MISRA C:2012 status

Checked with `make -C stm32_cluster misra` (cppcheck 2.22 MISRA addon) over every STM32 and common source file except the startup file. The run reports 147 findings. They are listed below by rule, with the decision taken for each.

## Accepted deviations

| Rule | Category | Count | Where | Rationale |
| --- | --- | --- | --- | --- |
| 15.5 (single point of exit) | Advisory | 46 | guard clauses | Early returns on invalid input keep the functions flat and readable. Each early return happens before any state change. |
| 8.7 (could be static) | Advisory | 34 | public module APIs | Functions are part of a module interface that is also used by the host tests or the CLI. |
| 17.1 (stdarg.h) | Required | 13 | `services/log.c` only | The debug formatter needs variadic arguments. It is compiled with `-Wformat=2` and the `format(printf)` attribute, so every call site is type-checked by the compiler. It is not used in safety-relevant paths. |
| 2.5 (unused macro) | Advisory | 11 | register and protocol constants | Constants that document the protocol (for example NRC codes) are kept even if not every one is referenced. |
| 17.8 (parameter modified) | Advisory | 9 | clipping helpers (`gfx`, `ili9341`) | Clipping the width/height parameters in place is clearer than copies. |
| 8.9 (block scope object) | Advisory | 5 | static `const` tables | Tables are kept next to the only function that uses them. |
| 11.5 (cast from void pointer) | Advisory | 1 | `nvm_write` | The NVM API takes `const void *` for any record type, and the bytes are only copied. |

## Open findings (planned cleanup)

| Rule | Count | Where | Planned fix |
| --- | --- | --- | --- |
| 10.3 / 10.7 / 10.8 (essential type model) | 24 | `cluster.c`, `dashboard.c`, `gfx.c`, `nvm.c`, `wdg_m.c`, `uds.c` | Explicit casts on composite expressions narrowing to `uint16_t`/`uint8_t`. These do not change behaviour (the values are range-limited), but the casts should be added. |
| 12.1 / 12.2 / 13.5 | 4 | `gfx.c`, `uds.c` | Extra parentheses and an explicit shift-range comment. |

## Not checked

- `startup/startup_stm32f103.c` uses linker symbols and a function-pointer table, which deviate from 11.1 by design.
- `stm32_cluster/cmsis` is third-party code.
- The ESP32 project uses ESP-IDF APIs and is not part of the MISRA scope.
