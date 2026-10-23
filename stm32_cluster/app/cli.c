/**
 * @file    cli.c
 * @brief   Debug console (see cli.h).
 */
#include "cli.h"
#include "app_info.h"
#include "uart.h"
#include "log.h"
#include "sched.h"
#include "bxcan.h"
#include "com.h"
#include "diag.h"
#include "dem.h"
#include "wdg_m.h"
#include "cluster.h"
#include "dashboard.h"
#include "adc.h"
#include "board.h"
#include <string.h>

#define CLI_LINE_MAX    (48U)

static char    s_line[CLI_LINE_MAX];
static uint8_t s_len;

static void cmd_help(void)
{
    log_printf("commands: help stats can dtc clear uptime page reset wdgtest fault\r\n");
}

static void cmd_stats(void)
{
    log_printf("CPU load %u%% (peak %u%%), stack used %lu/%lu B, uart drops %lu\r\n",
               (unsigned)sched_cpu_load_pct(), (unsigned)sched_cpu_load_peak_pct(),
               (unsigned long)app_stack_used_bytes(), (unsigned long)app_stack_total_bytes(),
               (unsigned long)uart_tx_dropped());
    log_printf("    task period  last_us  wcet_us     runs  overruns\r\n");
    for (uint8_t i = 0U; i < sched_task_count(); i++)
    {
        const sched_task_t *t = sched_task(i);
        log_printf("%8s %4lums %8lu %8lu %8lu %9lu\r\n", t->name, (unsigned long)t->period_ms,
                   (unsigned long)sched_cycles_to_us(t->last_cycles),
                   (unsigned long)sched_cycles_to_us(t->wcet_cycles),
                   (unsigned long)t->runs, (unsigned long)t->overruns);
    }
}

static void cmd_can(void)
{
    can_stats_t cs;
    com_stats_t ms;
    const isotp_stats_t *tp = diag_isotp_stats();

    can_get_stats(&cs);
    com_get_stats(&ms);
    log_printf("bxCAN rx %lu tx %lu  TEC %u REC %u LEC %u  %s  busoff events %lu\r\n",
               (unsigned long)cs.rx_frames, (unsigned long)cs.tx_frames, (unsigned)cs.tec,
               (unsigned)cs.rec, (unsigned)cs.last_error_code,
               cs.bus_off ? "BUS-OFF" : (cs.error_passive ? "ERROR-PASSIVE" : "ERROR-ACTIVE"),
               (unsigned long)cs.bus_off_events);
    log_printf("queues: sw overflow %lu, fifo overrun %lu, no mailbox %lu\r\n",
               (unsigned long)cs.rx_queue_overflows, (unsigned long)cs.rx_fifo_overruns,
               (unsigned long)cs.tx_no_mailbox);
    log_printf("COM engine %lu body %lu  crc err %lu seq err %lu  timeouts eng %lu body %lu\r\n",
               (unsigned long)ms.engine_rx, (unsigned long)ms.body_rx,
               (unsigned long)ms.e2e_crc_errors, (unsigned long)ms.e2e_seq_errors,
               (unsigned long)ms.engine_timeouts, (unsigned long)ms.body_timeouts);
    log_printf("ISO-TP rx ok %u err %u  tx ok %u err %u\r\n", (unsigned)tp->rx_ok,
               (unsigned)tp->rx_errors, (unsigned)tp->tx_ok, (unsigned)tp->tx_errors);
}

static void cmd_dtc(void)
{
    static const char *const names[DEM_EVT_COUNT] = {
        "U0100 engine msg lost", "U0140 body msg lost", "U0001 CAN bus-off",
        "U0401 engine E2E error", "B1000 watchdog reset", "B1002 speed implausible"
    };
    log_printf("DTC      status  occ  description\r\n");
    for (uint8_t i = 0U; i < (uint8_t)DEM_EVT_COUNT; i++)
    {
        const dem_event_t e = (dem_event_t)i;
        log_printf("%06lX   0x%02X  %3u  %s\r\n", (unsigned long)dem_dtc_code(e),
                   (unsigned)dem_status(e), (unsigned)dem_occurrences(e), names[i]);
    }
}

static void execute(const char *cmd, uint32_t now_ms)
{
    if (strcmp(cmd, "help") == 0)         { cmd_help(); }
    else if (strcmp(cmd, "stats") == 0)   { cmd_stats(); }
    else if (strcmp(cmd, "can") == 0)     { cmd_can(); }
    else if (strcmp(cmd, "dtc") == 0)     { cmd_dtc(); }
    else if (strcmp(cmd, "clear") == 0)   { dem_clear_all(); sched_reset_stats(); log_printf("cleared\r\n"); }
    else if (strcmp(cmd, "uptime") == 0)  { log_printf("%lu.%03lu s\r\n", (unsigned long)(now_ms / 1000UL),
                                                       (unsigned long)(now_ms % 1000UL)); }
    else if (strcmp(cmd, "page") == 0)    { dashboard_next_info_page(); }
    else if (strcmp(cmd, "reset") == 0)   { NVIC_SystemReset(); }
    else if (strcmp(cmd, "wdgtest") == 0) { log_printf("stalling supervision -> IWDG reset\r\n"); wdg_m_test_stall(); }
    else if (strcmp(cmd, "fault") == 0)
    {
        /* Permanently undefined instruction -> UsageFault -> register dump -> reset */
        log_printf("executing UDF...\r\n");
        __asm volatile("udf #0");
    }
    else if (cmd[0] != '\0')              { log_printf("unknown: %s (try help)\r\n", cmd); }
    else                                  { /* empty line */ }
}

void cli_init(void)
{
    s_len = 0U;
    log_printf("\r\nCAN mini cluster " FW_VERSION_STR " - type 'help'\r\n> ");
}

void cli_poll(uint32_t now_ms)
{
    uint8_t c;
    while (uart_getc(&c))
    {
        if ((c == (uint8_t)'\r') || (c == (uint8_t)'\n'))
        {
            if (s_len > 0U)
            {
                s_line[s_len] = '\0';
                log_printf("\r\n");
                execute(s_line, now_ms);
                s_len = 0U;
                log_printf("> ");
            }
        }
        else if ((c == 0x08U) || (c == 0x7FU))      /* backspace */
        {
            if (s_len > 0U)
            {
                s_len--;
                log_printf("\b \b");
            }
        }
        else if ((s_len < (CLI_LINE_MAX - 1U)) && (c >= 0x20U) && (c < 0x7FU))
        {
            s_line[s_len] = (char)c;
            s_len++;
            (void)uart_write(&c, 1U);               /* echo */
        }
        else
        {
            /* ignore */
        }
    }
}
