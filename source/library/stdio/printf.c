/**
 * @file printf.c
 * printf() implementation for bare-metal S5PV210
 * Outputs to serial port 0 (debug UART)
 * Required by LVGL's LV_LOG_PRINTF
 */

#include <stdio.h>
#include <stdarg.h>
#include <types.h>
#include <malloc.h>
#include <s5pv210-serial.h>

int printf(const char *fmt, ...)
{
    va_list ap;
    char *buf;
    int len;
    int rv;

    va_start(ap, fmt);
    len = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    if(len < 0)
        return 0;

    buf = malloc(len + 1);
    if(!buf)
        return 0;

    va_start(ap, fmt);
    rv = vsnprintf(buf, len + 1, fmt, ap);
    va_end(ap);

    rv = (s5pv210_serial_write_string(0, buf) < 0) ? 0 : rv;
    free(buf);

    return rv;
}

int vprintf(const char *fmt, va_list ap)
{
    char *buf;
    int len;
    int rv;
    va_list ap_copy;

    va_copy(ap_copy, ap);
    len = vsnprintf(NULL, 0, fmt, ap_copy);
    va_end(ap_copy);

    if(len < 0)
        return 0;

    buf = malloc(len + 1);
    if(!buf)
        return 0;

    rv = vsnprintf(buf, len + 1, fmt, ap);
    rv = (s5pv210_serial_write_string(0, buf) < 0) ? 0 : rv;
    free(buf);

    return rv;
}
