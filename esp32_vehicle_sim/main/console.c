/**
 * @file    console.c
 * @brief   UART0 command console (see console.h).
 */
#include "console.h"
#include "app_config.h"
#include "can_if.h"
#include "sim.h"
#include "tester.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define CONSOLE_UART    UART_NUM_0
#define CONSOLE_LINE_MAX        (128)

static void help(void)
{
    printf(
        "vehicle:  mode sweep|manual   speed <kmh>   rpm <n>   temp <degC>\n"
        "          turn l|r|off   beam 0|1   door 0|1   belt 0|1\n"
        "faults:   mute engine|body|none   crc on|off   pulse on|off   pulse_offset <kmh>\n"
        "diag:     uds <hex bytes>   vin   ver   dtc   ext   clear   reset   tp on|off\n"
        "misc:     log on|off (candump)   status   help\n");
}

static bool parse_hex(char *args, uint8_t *out, uint16_t *len)
{
    uint16_t n = 0U;
    for (char *tok = strtok(args, " "); tok != NULL; tok = strtok(NULL, " "))
    {
        char *end;
        const unsigned long v = strtoul(tok, &end, 16);
        if ((*end != '\0') || (v > 0xFFUL) || (n >= TESTER_MAX_REQ))
        {
            return false;
        }
        out[n++] = (uint8_t)v;
    }
    *len = n;
    return n > 0U;
}

static void send_uds(const uint8_t *req, uint16_t len)
{
    printf("UDS -> ");
    for (uint16_t i = 0; i < len; i++)
    {
        printf("%02X ", req[i]);
    }
    printf("\n");
    if (!tester_request(req, len))
    {
        printf("tester busy\n");
    }
}

static bool on_off(const char *a)
{
    return (a != NULL) && ((strcmp(a, "on") == 0) || (strcmp(a, "1") == 0));
}

static void execute(char *line)
{
    sim_cfg_t *c = sim_cfg();
    char *cmd = strtok(line, " ");
    char *arg = strtok(NULL, "");      /* rest of the line */

    if (cmd == NULL)                         { return; }
    if (strcmp(cmd, "help") == 0)            { help(); }
    else if (strcmp(cmd, "mode") == 0)       { c->mode = (arg && strcmp(arg, "manual") == 0) ? SIM_MODE_MANUAL : SIM_MODE_SWEEP; }
    else if (strcmp(cmd, "speed") == 0 && arg) { c->mode = SIM_MODE_MANUAL; c->speed_kmh = strtof(arg, NULL); }
    else if (strcmp(cmd, "rpm") == 0 && arg)   { c->mode = SIM_MODE_MANUAL; c->rpm = strtof(arg, NULL); }
    else if (strcmp(cmd, "temp") == 0 && arg)  { c->mode = SIM_MODE_MANUAL; c->coolant_c = strtof(arg, NULL); }
    else if (strcmp(cmd, "turn") == 0 && arg)
    {
        c->mode = SIM_MODE_MANUAL;
        c->turn_left  = (strcmp(arg, "l") == 0);
        c->turn_right = (strcmp(arg, "r") == 0);
    }
    else if (strcmp(cmd, "beam") == 0)         { c->mode = SIM_MODE_MANUAL; c->high_beam = on_off(arg); }
    else if (strcmp(cmd, "door") == 0)         { c->mode = SIM_MODE_MANUAL; c->door_open = on_off(arg); }
    else if (strcmp(cmd, "belt") == 0)         { c->mode = SIM_MODE_MANUAL; c->belt_unfastened = on_off(arg); }
    else if (strcmp(cmd, "mute") == 0 && arg)
    {
        c->mute_engine = (strcmp(arg, "engine") == 0);
        c->mute_body   = (strcmp(arg, "body") == 0);
    }
    else if (strcmp(cmd, "crc") == 0)          { c->corrupt_crc = on_off(arg); }
    else if (strcmp(cmd, "pulse") == 0)        { c->pulse_enabled = on_off(arg); }
    else if (strcmp(cmd, "pulse_offset") == 0 && arg) { c->pulse_offset_kmh = strtof(arg, NULL); }
    else if (strcmp(cmd, "log") == 0)          { can_if_set_logging(on_off(arg)); }
    else if (strcmp(cmd, "status") == 0)       { can_if_print_status(); }
    else if (strcmp(cmd, "tp") == 0)           { tester_keepalive(on_off(arg)); }
    else if (strcmp(cmd, "uds") == 0 && arg)
    {
        uint8_t req[TESTER_MAX_REQ];
        uint16_t len;
        if (parse_hex(arg, req, &len)) { send_uds(req, len); }
        else                           { printf("usage: uds 22 F1 90\n"); }
    }
    else if (strcmp(cmd, "vin") == 0)   { const uint8_t r[] = { 0x22, 0xF1, 0x90 }; send_uds(r, sizeof(r)); }
    else if (strcmp(cmd, "ver") == 0)   { const uint8_t r[] = { 0x22, 0xF1, 0x95 }; send_uds(r, sizeof(r)); }
    else if (strcmp(cmd, "dtc") == 0)   { const uint8_t r[] = { 0x19, 0x02, 0xFF }; send_uds(r, sizeof(r)); }
    else if (strcmp(cmd, "ext") == 0)   { const uint8_t r[] = { 0x10, 0x03 };       send_uds(r, sizeof(r)); }
    else if (strcmp(cmd, "clear") == 0) { const uint8_t r[] = { 0x14, 0xFF, 0xFF, 0xFF }; send_uds(r, sizeof(r)); }
    else if (strcmp(cmd, "reset") == 0) { const uint8_t r[] = { 0x11, 0x01 };       send_uds(r, sizeof(r)); }
    else                                { printf("unknown command, try 'help'\n"); }
}

static void console_task(void *arg)
{
    (void)arg;
    char line[CONSOLE_LINE_MAX];
    int len = 0;
    uint8_t ch;

    help();
    printf("> ");
    fflush(stdout);
    for (;;)
    {
        if (uart_read_bytes(CONSOLE_UART, &ch, 1, portMAX_DELAY) != 1)
        {
            continue;
        }
        if ((ch == '\r') || (ch == '\n'))
        {
            printf("\n");
            if (len > 0)
            {
                line[len] = '\0';
                execute(line);
                len = 0;
            }
            printf("> ");
        }
        else if (((ch == 0x08) || (ch == 0x7F)) && (len > 0))
        {
            len--;
            printf("\b \b");
        }
        else if ((len < (CONSOLE_LINE_MAX - 1)) && isprint(ch))
        {
            line[len++] = (char)ch;
            putchar(ch);
        }
        fflush(stdout);
    }
}

void console_init(void)
{
    ESP_ERROR_CHECK(uart_driver_install(CONSOLE_UART, 256, 0, 0, NULL, 0));
    xTaskCreate(console_task, "console", 4096, NULL, TASK_PRIO_CONSOLE, NULL);
}
