#pragma once

#include "../../core/core.h"
#include "../common/config.h"

/**
 * @returns 1 if directory_path is a directory, 0 otherwise.
 */
int dir_exists(const char *directory_path);

/**
 * Creates directory_path and its parents if they don't exist.
 */
bool mkdirp(const char *directory_path);

/**
 * Makes all parent dirs of filepath if necessary
 */
bool make_parent_dirs(const char *filepath);

uint8_t *read_file(const char *path, size_t *len);

bool write_file(const char *path, const uint8_t *data, size_t len);

bool save_battery_to_file(gbmulator_t *emu, const char *path);

bool load_battery_from_file(gbmulator_t *emu, const char *path);

bool save_state_to_file(gbmulator_t *emu, const char *path, int compressed);

bool load_state_from_file(gbmulator_t *emu, const char *path);

/**
 * @returns the contents of the file at `path` or `NULL` if the contents are invalid or an unsupported ROM.
 *          gb_init() won't fail if the return value is not `NULL`.
 */
uint8_t *read_rom(const char *path, size_t *rom_size);

char *get_config_dir(void);

char *get_save_dir(void);

char *get_savestate_dir(void);

char *get_config_path(void);

char *get_save_path(const char *rom_title);

char *get_savestate_path(const char *rom_title, int slot);

/**
 * resize, crop (keep center) and rotate `src` to fit into `dst` where `dst` is the gb camera sensor buffer
 */
void fit_image(uint8_t *dst, const uint8_t *src, int src_width, int src_height, int row_stride, int rotation);

// TODO remove?
uint8_t *read_file_f(FILE *f, size_t *len);
