#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "log.h"

#ifdef __GNUC__
#define UNUSED __attribute__((unused))
#else
#define UNUSED
#endif

#define CHECK_BIT(var, pos)           ((var) & (1 << (pos)))
#define SET_BIT(var, pos)             ((var) |= (1 << (pos)))
#define RESET_BIT(var, pos)           ((var) &= ~(1 << (pos)))
#define GET_BIT(var, pos)             (((var) >> (pos)) & 1)
#define TOGGLE_BIT(var, pos)          ((var) ^= 1UL << (pos))
#define CHANGE_BIT(var, pos, value)   ((var) ^= (-(value) ^ (var)) & (1UL << (pos)))
#define CHANGE_BITS(var, mask, value) ((var) = (((var) & (~(mask))) | ((value) & (mask))))

#define ALIGN(x, n) ((x) & (~((n) - 1)))

#ifndef MIN
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif

#ifndef MAX
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

#ifndef CLAMP
#define CLAMP(x, lo, hi) MAX(MIN((x), (hi)), (lo))
#endif

#define XSTRINGIFY(x) #x
#define STRINGIFY(x)  XSTRINGIFY(x)

#define todo(format, ...)                           \
    do {                                            \
        LOG_ERROR("TODO - " format, ##__VA_ARGS__); \
        exit(42);                                   \
    } while (0)

#if __has_include(<stdbit.h>)
#include <stdbit.h>
#else
static inline uint8_t stdc_count_ones_impl(uint32_t v) {
    return __builtin_popcount(v);
}

static inline uint8_t stdc_first_trailing_one_impl(uint32_t v) {
    return v == 0 ? 0 : 1 + __builtin_ctz(v);
}

#define stdc_count_ones         stdc_count_ones_impl
#define stdc_first_trailing_one stdc_first_trailing_one_impl
#endif

void *xmalloc(size_t size);

void *xcalloc(size_t nmemb, size_t size);

void *xrealloc(void *ptr, size_t size);
