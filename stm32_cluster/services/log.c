/**
 * @file    log.c
 * @brief   Minimal formatter (see log.h).
 */
#include "log.h"
#include "uart.h"
#include <stdarg.h>

#define LOG_LINE_MAX    (160U)

typedef struct
{
    char    *buf;
    uint16_t size;
    uint16_t len;
} out_t;

static void out_c(out_t *o, char c)
{
    if ((uint16_t)(o->len + 1U) < o->size)
    {
        o->buf[o->len] = c;
        o->len++;
    }
}

static void out_num(out_t *o, uint32_t v, uint8_t base, bool upper, bool neg,
                    uint8_t width, char pad)
{
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char tmp[11];
    uint8_t n = 0U;

    do
    {
        tmp[n] = digits[v % base];
        n++;
        v /= base;
    } while ((v != 0UL) && (n < sizeof(tmp)));

    const uint8_t total = (uint8_t)(n + (neg ? 1U : 0U));
    if (neg && (pad == '0'))
    {
        out_c(o, '-');
    }
    for (uint8_t i = total; i < width; i++)
    {
        out_c(o, pad);
    }
    if (neg && (pad != '0'))
    {
        out_c(o, '-');
    }
    while (n > 0U)
    {
        n--;
        out_c(o, tmp[n]);
    }
}

static void vformat(out_t *o, const char *fmt, va_list ap)
{
    while (*fmt != '\0')
    {
        if (*fmt != '%')
        {
            out_c(o, *fmt);
            fmt++;
            continue;
        }
        fmt++;

        char pad = ' ';
        uint8_t width = 0U;
        if (*fmt == '0')
        {
            pad = '0';
            fmt++;
        }
        while ((*fmt >= '0') && (*fmt <= '9'))
        {
            width = (uint8_t)((width * 10U) + (uint8_t)(*fmt - '0'));
            fmt++;
        }
        while (*fmt == 'l')
        {
            fmt++;
        }

        switch (*fmt)
        {
        case 'd':
        {
            const int32_t v = va_arg(ap, int32_t);
            const uint32_t mag = (v < 0) ? (uint32_t)(-(int64_t)v) : (uint32_t)v;
            out_num(o, mag, 10U, false, v < 0, width, pad);
            break;
        }
        case 'u': out_num(o, va_arg(ap, uint32_t), 10U, false, false, width, pad); break;
        case 'x': out_num(o, va_arg(ap, uint32_t), 16U, false, false, width, pad); break;
        case 'X': out_num(o, va_arg(ap, uint32_t), 16U, true, false, width, pad);  break;
        case 'c': out_c(o, (char)va_arg(ap, int));                                 break;
        case 's':
        {
            const char *s = va_arg(ap, const char *);
            uint8_t n = 0U;
            while (s[n] != '\0') { n++; }
            for (uint8_t i = n; i < width; i++) { out_c(o, ' '); }
            while (*s != '\0') { out_c(o, *s); s++; }
            break;
        }
        case '%': out_c(o, '%'); break;
        case '\0': return;
        default:  out_c(o, '?'); break;
        }
        fmt++;
    }
}

uint16_t log_snprintf(char *buf, uint16_t size, const char *fmt, ...)
{
    out_t o = { buf, size, 0U };
    va_list ap;
    va_start(ap, fmt);
    vformat(&o, fmt, ap);
    va_end(ap);
    if (size > 0U)
    {
        buf[o.len] = '\0';
    }
    return o.len;
}

void log_printf(const char *fmt, ...)
{
    char line[LOG_LINE_MAX];
    out_t o = { line, (uint16_t)sizeof(line), 0U };
    va_list ap;
    va_start(ap, fmt);
    vformat(&o, fmt, ap);
    va_end(ap);
    (void)uart_write((const uint8_t *)line, o.len);
}
