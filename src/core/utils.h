#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#ifdef __GNUC__
#   define UNUSED __attribute__((unused))
#else
#   define UNUSED
#endif

#define CHECK_BIT(var, pos) ((var) & (1 << (pos)))
#define SET_BIT(var, pos) ((var) |= (1 << (pos)))
#define RESET_BIT(var, pos) ((var) &= ~(1 << (pos)))
#define GET_BIT(var, pos) (((var) >> (pos)) & 1)
#define TOGGLE_BIT(var, pos) ((var) ^= 1UL << (pos))
#define CHANGE_BIT(var, pos, value) ((var) ^= (-(value) ^ (var)) & (1UL << (pos)))
#define CHANGE_BITS(var, mask, value) ((var) = (((var) & (~(mask))) | ((value) & (mask))))

#define ALIGN(x, n) ((x) & (~((n) - 1)))

#ifndef MIN
#   define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif

#ifndef MAX
#   define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

#ifndef CLAMP
#   define CLAMP(x, lo, hi) MAX(MIN((x), (hi)), (lo))
#endif

#define XSTRINGIFY(x) #x
#define STRINGIFY(x) XSTRINGIFY(x)

#define eprintf(format, ...) fprintf(stderr, "[ERROR] %s:%d - %s() - "format"\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#define errnoprintf(format, ...) eprintf(format": %s", ##__VA_ARGS__, strerror(errno));

#define todo(format, ...)               \
    do {                                \
        eprintf(format, ##__VA_ARGS__); \
        exit(42);                       \
    } while (0)

#ifdef DEBUG
#   define LOG_DEBUG(...) fprintf(stderr, __VA_ARGS__)
#else
#   define LOG_DEBUG(...)
#endif

void *xmalloc(size_t size);

void *xcalloc(size_t nmemb, size_t size);

void *xrealloc(void *ptr, size_t size);
