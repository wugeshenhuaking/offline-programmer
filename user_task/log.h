/**
 * log.h - 单文件 C51 日志库，LOG_TAG 自动转大写输出
 * 注意：日志会自动换行，请勿在格式字符串末尾添加 \n
 * 正确：log_info("value=%d", x);
 * 错误：log_info("value=%d\n", x);  ← 会导致空行！

 * 使用方式：
 *   在 .c 文件顶部定义 LOG_TAG 和 LOG_MODULE_LVL，然后 #include "log.h"
 * 示例：
 *   #define LOG_TAG "MAIN"
 *   #define LOG_MODULE_LVL LOG_LVL_DEBUG
 *   #include "log.h"
 * author: RickWu
 * version:
 *   V1.0.0 2026_01_07
 *      first verison
 */

#ifndef __LOG_H__
#define __LOG_H__

#include <stdio.h>
#include <stdarg.h>

// ====================== 日志级别 ======================
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
    #warning "LOG_TAG not defined, using 'unknown'"
    #define LOG_TAG "UNKNOWN"
#endif

#ifndef LOG_MODULE_LVL
    #define LOG_MODULE_LVL LOG_LVL_NONE
#endif

// ====================== 辅助：输出大写字符串 ======================
static void _print_upper_str(const char *s)
{
    char c;
    while ((c = *s++) != '\0')
    {
        if (c >= 'a' && c <= 'z')
            c = c - 'a' + 'A';  // 转大写
        putchar(c);
    }
}

// ====================== 内部：通用输出函数（TAG 自动大写） ======================
static void _log_vprintf(const char *level, const char *fmt, va_list args)
{
    putchar('[');
    _print_upper_str(LOG_TAG);  // ← 关键：这里输出大写
    printf("] %s: ", level);
    vprintf(fmt, args);
    printf("\r\n");
}

// ====================== 空函数 ======================
static void _log_null(const char *fmt, ...)
{
    // 空实现S
  if(fmt)
    return ;
  return ;
}

// ====================== 日志接口 ======================

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

#endif // __LOG_H__
