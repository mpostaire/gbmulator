#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#ifndef LOG_LEVEL
#define LOG_LEVEL info
#endif

// internal log values
#define LOG_LVL_DEBUG 0
#define LOG_LVL_INFO  1
#define LOG_LVL_WARN  2
#define LOG_LVL_ERROR 3

// map log values to internal log values
#define _LOG_LVL_IMPL_0 LOG_LVL_DEBUG
#define _LOG_LVL_IMPL_1 LOG_LVL_INFO
#define _LOG_LVL_IMPL_2 LOG_LVL_WARN
#define _LOG_LVL_IMPL_3 LOG_LVL_ERROR

// map log names to internal log values
#define _LOG_LVL_IMPL_debug LOG_LVL_DEBUG
#define _LOG_LVL_IMPL_info  LOG_LVL_INFO
#define _LOG_LVL_IMPL_warn  LOG_LVL_WARN
#define _LOG_LVL_IMPL_error LOG_LVL_ERROR

// helpers
#define _LOG_CONCAT_IMPL(a, b) a##b
#define _LOG_CONCAT(a, b)      _LOG_CONCAT_IMPL(a, b)
#define _LOG_EXPAND(x)         x

#define _LOG_LVL_VALUE \
    _LOG_EXPAND(_LOG_CONCAT(_LOG_LVL_IMPL_, LOG_LEVEL))

#define _LOG_COLOR_RED    "\x1b[31m"
#define _LOG_COLOR_YELLOW "\x1b[33m"
#define _LOG_COLOR_GRAY   "\x1b[90m"
#define _LOG_COLOR_RESET  "\x1b[0m"
#define _LOG_COLOR(level)                                                                     \
    ((level) == LOG_LVL_DEBUG ? _LOG_COLOR_GRAY : (level) == LOG_LVL_WARN ? _LOG_COLOR_YELLOW \
                                              : (level) == LOG_LVL_ERROR  ? _LOG_COLOR_RED    \
                                                                          : "")

// log with file, line and function information
#define _LOG_FULL_IMPL(lvl, lvl_str, fmt, ...)                               \
    do {                                                                     \
        if (_log_is_colored()) {                                             \
            fprintf(stderr, "%s[%s] %s:%d:%s(): " fmt _LOG_COLOR_RESET "\n", \
                    _LOG_COLOR(lvl),                                         \
                    lvl_str,                                                 \
                    __FILE__,                                                \
                    __LINE__,                                                \
                    __func__,                                                \
                    ##__VA_ARGS__);                                          \
        } else {                                                             \
            fprintf(stderr, "[%s] %s:%d:%s(): " fmt "\n",                    \
                    lvl_str,                                                 \
                    __FILE__,                                                \
                    __LINE__,                                                \
                    __func__,                                                \
                    ##__VA_ARGS__);                                          \
        }                                                                    \
    } while (0)

// log message only
#define _LOG_IMPL(lvl, lvl_str, fmt, ...)                        \
    do {                                                         \
        if (_log_is_colored()) {                                 \
            fprintf(stderr, "%s[%s] " fmt _LOG_COLOR_RESET "\n", \
                    _LOG_COLOR(lvl),                             \
                    lvl_str,                                     \
                    ##__VA_ARGS__);                              \
        } else {                                                 \
            fprintf(stderr, "[%s] " fmt "\n",                    \
                    lvl_str,                                     \
                    ##__VA_ARGS__);                              \
        }                                                        \
    } while (0)

#if _LOG_LVL_VALUE <= LOG_LVL_DEBUG
#define LOG_DEBUG(fmt, ...) _LOG_IMPL(LOG_LVL_DEBUG, "D", fmt, ##__VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...)
#endif

#if _LOG_LVL_VALUE <= LOG_LVL_INFO
#define LOG_INFO(fmt, ...) _LOG_IMPL(LOG_LVL_INFO, "I", fmt, ##__VA_ARGS__)
#else
#define LOG_INFO(fmt, ...)
#endif

#if _LOG_LVL_VALUE <= LOG_LVL_WARN
#define LOG_WARN(fmt, ...) _LOG_FULL_IMPL(LOG_LVL_WARN, "W", fmt, ##__VA_ARGS__)
#else
#define LOG_WARN(fmt, ...)
#endif

#if _LOG_LVL_VALUE <= LOG_LVL_ERROR
#define LOG_ERROR(fmt, ...) _LOG_FULL_IMPL(LOG_LVL_ERROR, "E", fmt, ##__VA_ARGS__)
#else
#define LOG_ERROR(fmt, ...)
#endif

static inline int _log_is_colored(void) {
    static int log_use_color = -1;
    if (log_use_color == -1)
        log_use_color = isatty(STDERR_FILENO) && getenv("NO_COLOR") == NULL;
    return log_use_color;
}
