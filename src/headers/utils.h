#pragma once

#define CHECK_BIT(var, pos) ((var) & (1 << (pos)))
#define SET_BIT(var, pos) ((var) |= (1 << (pos)))
#define RESET_BIT(var, pos) ((var) &= ~(1 << (pos)))
#define GET_BIT(var, pos) (((var) >> (pos)) & 1)

/**
 * @returns 1 if directory_path is a directory, 0 otherwise.
 */
int dir_exists(const char *directory_path);

/**
 * Creates directory_path and its parents if they don't exist.
 */
void mkdirp(const char *directory_path);
