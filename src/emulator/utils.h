#pragma once

#include <stdio.h>
#include <errno.h>

#define CHECK_BIT(var, pos) ((var) & (1 << (pos)))
#define SET_BIT(var, pos) ((var) |= (1 << (pos)))
#define RESET_BIT(var, pos) ((var) &= ~(1 << (pos)))
#define GET_BIT(var, pos) (((var) >> (pos)) & 1)
#define TOGGLE_BIT(var, pos) ((var) ^= 1UL << (pos))
#define CHANGE_BIT(var, value, pos) ((var) ^= (-(value) ^ (var)) & (1UL << (pos)))

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define CLAMP(x, lo, hi) MAX(MIN((x), (hi)), (lo))

#define eprintf(format, ...) fprintf(stderr, "ERROR - %s() - "format, __func__, ##__VA_ARGS__)
#define errnoprintf(format, ...) eprintf(format": %s\n", ##__VA_ARGS__, strerror(errno));
#define errnoprint() fprintf(stderr, "ERROR - %s() - %s\n", __func__, strerror(errno))
