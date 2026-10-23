/**
 * @file    dashboard.c
 * @brief   Instrument cluster screen (see dashboard.h).
 *
 * Layout (landscape 320x240):
 *   y   0..15   telltales  <  HI  DOOR  BELT  CHK  >
 *   y  40..95   speed, 3 digits at scale 7 (56x56 px each), "km/h" + source tag
 *   y 112..127  RPM bar      0..8000 rpm
 *   y 140..155  TEMP bar    40..130 degC
 *   y 168..183  FUEL bar     0..100 %
 *   y 200..215  info line (MENU button cycles ODO / TRIP / CAN / SYSTEM)
 *   y 228..235  link status line
 *
 * Every widget caches what it last drew; nothing is redrawn unless its value or
 * colour changed. A full-screen repaint (~34 ms of SPI time) happens only at init.
 */
#include "dashboard.h"
#include "gfx.h"
#include "ili9341.h"
#include "log.h"
#include "bxcan.h"
#include "sched.h"
#include "can_matrix.h"
#include <string.h>

#define BG              COLOR_BLACK
#define FG              COLOR_WHITE
#define BLINK_HALF_MS   (333UL)     /* 1.5 Hz turn indicator */

#define BAR_X           (72U)
#define BAR_W           (176U)
#define BAR_H           (16U)
#define VAL_X           (256U)
#define ROW_RPM_Y       (112U)
#define ROW_TEMP_Y      (140U)
#define ROW_FUEL_Y      (168U)

#define RPM_MAX         (8000U)
#define RPM_RED         (6500U)
#define TEMP_MIN_C      (40)
#define TEMP_MAX_C      (130)

typedef struct
{
    char     text[24];
    uint16_t fg;
    bool     drawn;
} text_cache_t;

typedef struct
{
    uint16_t fill;
    uint16_t color;
    bool     drawn;
} bar_cache_t;

static text_cache_t s_tt[6];            /* telltales */
static text_cache_t s_speed;
static text_cache_t s_speed_tag;
static text_cache_t s_val[3];
static bar_cache_t  s_bar[3];
static text_cache_t s_info;
static text_cache_t s_link;
static info_page_t  s_page;
static info_page_t  s_drawn_page;

/* Redraw text only when its content or colour changed */
static void text_widget(text_cache_t *c, uint16_t x, uint16_t y, uint8_t scale,
                        const char *txt, uint16_t fg)
{
    if (c->drawn && (c->fg == fg) && (strncmp(c->text, txt, sizeof(c->text)) == 0))
    {
        return;
    }
    gfx_text(x, y, txt, scale, fg, BG);
    (void)strncpy(c->text, txt, sizeof(c->text) - 1U);
    c->text[sizeof(c->text) - 1U] = '\0';
    c->fg    = fg;
    c->drawn = true;
}

static void bar_widget(bar_cache_t *c, uint16_t y, uint32_t value, uint32_t max, uint16_t color)
{
    const uint16_t fill = (uint16_t)((value >= max) ? BAR_W : ((value * BAR_W) / max));

    if (!c->drawn || (c->color != color))
    {
        /* colour change (e.g. red zone): repaint the whole bar once */
        gfx_fill_rect(BAR_X, y, BAR_W, BAR_H, COLOR_DARK);
        gfx_bar_update(BAR_X, y, BAR_W, BAR_H, 0U, fill, color, COLOR_DARK);
    }
    else
    {
        gfx_bar_update(BAR_X, y, BAR_W, BAR_H, c->fill, fill, color, COLOR_DARK);
    }
    c->fill  = fill;
    c->color = color;
    c->drawn = true;
}

static void draw_telltales(const cluster_state_t *st, uint32_t now)
{
    const bool blink_on = ((now / BLINK_HALF_MS) & 1UL) == 0UL;
    const uint16_t off = COLOR_DARK;

    text_widget(&s_tt[0], 4U,   0U, 2U, "<",    (st->turn_left && blink_on) ? COLOR_GREEN : off);
    text_widget(&s_tt[1], 44U,  0U, 2U, "HI",   st->high_beam ? COLOR_BLUE : off);
    text_widget(&s_tt[2], 92U,  0U, 2U, "DOOR", st->door_open ? COLOR_RED : off);
    text_widget(&s_tt[3], 168U, 0U, 2U, "BELT", st->belt_unfastened ? COLOR_RED : off);
    text_widget(&s_tt[4], 244U, 0U, 2U, "CHK",  (st->dtc_count > 0U) ? COLOR_AMBER : off);
    text_widget(&s_tt[5], 300U, 0U, 2U, ">",    (st->turn_right && blink_on) ? COLOR_GREEN : off);
}

static void draw_speed(const cluster_state_t *st)
{
    char buf[8];
    uint16_t color = FG;

    if (st->speed_src == SPEED_SRC_NONE)
    {
        (void)strcpy(buf, "---");
        color = COLOR_GREY;
    }
    else
    {
        (void)log_snprintf(buf, sizeof(buf), "%3u", (unsigned)((st->speed_kmh > 999U) ? 999U : st->speed_kmh));
        if ((st->warnings & WARN_OVERSPEED) != 0U)
        {
            color = COLOR_RED;
        }
    }
    text_widget(&s_speed, 60U, 40U, 7U, buf, color);

    const char *tag = (st->speed_src == SPEED_SRC_WHEEL) ? "WHEEL" : "     ";
    text_widget(&s_speed_tag, 236U, 100U, 1U, tag, COLOR_AMBER);
}

static void draw_gauges(const cluster_state_t *st)
{
    char buf[8];

    /* RPM */
    if (st->rpm_valid)
    {
        (void)log_snprintf(buf, sizeof(buf), "%4u", (unsigned)st->rpm);
        bar_widget(&s_bar[0], ROW_RPM_Y, st->rpm, RPM_MAX, (st->rpm >= RPM_RED) ? COLOR_RED : COLOR_CYAN);
    }
    else
    {
        (void)strcpy(buf, "----");
        bar_widget(&s_bar[0], ROW_RPM_Y, 0U, RPM_MAX, COLOR_GREY);
    }
    text_widget(&s_val[0], VAL_X, ROW_RPM_Y, 2U, buf, FG);

    /* Coolant temperature */
    if (st->coolant_valid)
    {
        const int32_t c = st->coolant_c;
        const uint32_t above_min = (c > TEMP_MIN_C) ? (uint32_t)(c - TEMP_MIN_C) : 0UL;
        (void)log_snprintf(buf, sizeof(buf), "%3dC", (int)c);
        bar_widget(&s_bar[1], ROW_TEMP_Y, above_min, (uint32_t)(TEMP_MAX_C - TEMP_MIN_C),
                   ((st->warnings & WARN_COOLANT_HIGH) != 0U) ? COLOR_RED : COLOR_GREEN);
    }
    else
    {
        (void)strcpy(buf, "---C");
        bar_widget(&s_bar[1], ROW_TEMP_Y, 0U, 1U, COLOR_GREY);
    }
    text_widget(&s_val[1], VAL_X, ROW_TEMP_Y, 2U, buf, FG);

    /* Fuel (local sensor, always valid) */
    (void)log_snprintf(buf, sizeof(buf), "%3u%%", (unsigned)st->fuel_pct);
    bar_widget(&s_bar[2], ROW_FUEL_Y, st->fuel_pct, 100U,
               ((st->warnings & WARN_LOW_FUEL) != 0U) ? COLOR_AMBER : COLOR_GREEN);
    text_widget(&s_val[2], VAL_X, ROW_FUEL_Y, 2U, buf, ((st->warnings & WARN_LOW_FUEL) != 0U) ? COLOR_AMBER : FG);
}

static void draw_info(const cluster_state_t *st)
{
    char buf[24];
    can_stats_t cs;

    if (s_page != s_drawn_page)
    {
        /* pages differ in length: clear the row so no characters of the old page remain */
        gfx_fill_rect(0U, 200U, LCD_WIDTH, 16U, BG);
        s_info.drawn = false;
        s_drawn_page = s_page;
    }

    switch (s_page)
    {
    case INFO_ODO:
        (void)log_snprintf(buf, sizeof(buf), "ODO %7lu.%lu km  ",
                           (unsigned long)(st->odometer_x10 / 10UL), (unsigned long)(st->odometer_x10 % 10UL));
        break;
    case INFO_TRIP:
        (void)log_snprintf(buf, sizeof(buf), "TRIP %5lu.%lu km  ",
                           (unsigned long)(st->trip_x10 / 10UL), (unsigned long)(st->trip_x10 % 10UL));
        break;
    case INFO_CAN:
        can_get_stats(&cs);
        (void)log_snprintf(buf, sizeof(buf), "TEC%3u REC%3u %s  ", (unsigned)cs.tec, (unsigned)cs.rec,
                           cs.bus_off ? "OFF" : (cs.error_passive ? "EP " : "OK "));
        break;
    default:
        (void)log_snprintf(buf, sizeof(buf), "DTC %u  CPU %2u%%    ", (unsigned)st->dtc_count,
                           (unsigned)sched_cpu_load_pct());
        break;
    }
    text_widget(&s_info, 8U, 200U, 2U, buf, FG);
}

static void draw_link(const cluster_state_t *st)
{
    char buf[40];
    (void)log_snprintf(buf, sizeof(buf), "ENGINE %s   BODY %s",
                       st->rpm_valid ? "OK  " : "LOST", st->body_valid ? "OK  " : "LOST");
    text_widget(&s_link, 8U, 228U, 1U, buf, (st->rpm_valid && st->body_valid) ? COLOR_GREY : COLOR_RED);
}

void dashboard_init(void)
{
    (void)memset(s_tt, 0, sizeof(s_tt));
    (void)memset(&s_speed, 0, sizeof(s_speed));
    (void)memset(&s_speed_tag, 0, sizeof(s_speed_tag));
    (void)memset(s_val, 0, sizeof(s_val));
    (void)memset(s_bar, 0, sizeof(s_bar));
    (void)memset(&s_info, 0, sizeof(s_info));
    (void)memset(&s_link, 0, sizeof(s_link));
    s_page = INFO_ODO;
    s_drawn_page = INFO_ODO;

    gfx_fill_rect(0U, 0U, LCD_WIDTH, LCD_HEIGHT, BG);
    gfx_text(236U, 80U, "km/h", 2U, COLOR_GREY, BG);
    gfx_text(4U, ROW_RPM_Y, "RPM", 2U, COLOR_GREY, BG);
    gfx_text(4U, ROW_TEMP_Y, "TMP", 2U, COLOR_GREY, BG);
    gfx_text(4U, ROW_FUEL_Y, "FUEL", 2U, COLOR_GREY, BG);
    gfx_fill_rect(0U, 192U, LCD_WIDTH, 1U, COLOR_DARK);
}

void dashboard_update(const cluster_state_t *st, uint32_t now_ms)
{
    draw_telltales(st, now_ms);
    draw_speed(st);
    draw_gauges(st);
    draw_info(st);
    draw_link(st);
}

void dashboard_next_info_page(void)
{
    s_page = (info_page_t)(((uint32_t)s_page + 1UL) % (uint32_t)INFO_COUNT);
}
