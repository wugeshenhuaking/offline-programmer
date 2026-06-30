/**
 * log.h - simple logging helpers
 * Notes:
 * 1. A trailing CRLF is appended automatically.
 * 2. LOG_TAG is printed in uppercase.
 */

#ifndef __LOG_H__
#define __LOG_H__

#include <stdio.h>
#include <stdarg.h>
#include "wk_system.h"

#define LOG_LVL_NONE    0
#define LOG_LVL_ERROR   1
#define LOG_LVL_WARN    2
#define LOG_LVL_INFO    3
#define LOG_LVL_DEBUG   4
#define LOG_LVL_VERBOSE 5

#ifndef LOG_GLOBAL_LVL
    #define LOG_GLOBAL_LVL LOG_LVL_DEBUG
#endif

#ifndef LOG_TAG
    #warning "LOG_TAG not defined, using 'UNKNOWN'"
    #define LOG_TAG "UNKNOWN"
#endif

#ifndef LOG_MODULE_LVL
    #define LOG_MODULE_LVL LOG_LVL_NONE
#endif

static void _print_upper_str(const char *s)
{
    char c;

    while ((c = *s++) != '\0')
    {
        if (c >= 'a' && c <= 'z')
        {
            c = (char)(c - 'a' + 'A');
        }
        putchar(c);
    }
}

static void _log_vprintf(const char *level, const char *fmt, va_list args)
{
    putchar('[');
    _print_upper_str(LOG_TAG);
    printf("] %s: ", level);
    vprintf(fmt, args);
    printf("\r\n");
}

static void _log_null(const char *fmt, ...)
{
    (void)fmt;
}

#if (LOG_GLOBAL_LVL >= LOG_LVL_ERROR) && (LOG_MODULE_LVL >= LOG_LVL_ERROR)
static void log_error(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    _log_vprintf("ERROR", fmt, args);
    va_end(args);
}
#else
    #define log_error _log_null
#endif

#if (LOG_GLOBAL_LVL >= LOG_LVL_WARN) && (LOG_MODULE_LVL >= LOG_LVL_WARN)
static void log_warn(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    _log_vprintf("WARN", fmt, args);
    va_end(args);
}
#else
    #define log_warn _log_null
#endif

#if (LOG_GLOBAL_LVL >= LOG_LVL_INFO) && (LOG_MODULE_LVL >= LOG_LVL_INFO)
static void log_info(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    _log_vprintf("INFO", fmt, args);
    va_end(args);
}
#else
    #define log_info _log_null
#endif

#if (LOG_GLOBAL_LVL >= LOG_LVL_DEBUG) && (LOG_MODULE_LVL >= LOG_LVL_DEBUG)
static void log_debug(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    _log_vprintf("DEBUG", fmt, args);
    va_end(args);
}
#else
    #define log_debug _log_null
#endif

#if (LOG_GLOBAL_LVL >= LOG_LVL_VERBOSE) && (LOG_MODULE_LVL >= LOG_LVL_VERBOSE)
static void log_verbose(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    _log_vprintf("VERBOSE", fmt, args);
    va_end(args);
}
#else
    #define log_verbose _log_null
#endif

#endif
