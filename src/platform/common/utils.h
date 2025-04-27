#pragma once

#include "../../core/core.h"
#include "../common/config.h"

joypad_button_t keycode_to_joypad(config_t *config, unsigned int keycode);

joypad_button_t button_to_joypad(config_t *config, unsigned int button);

/**
 * @returns 1 if directory_path is a directory, 0 otherwise.
 */
int dir_exists(const char *directory_path);

/**
 * Creates directory_path and its parents if they don't exist.
 */
void mkdirp(const char *directory_path);

/**
 * Makes all parent dirs of filepath if necessary
 */
void make_parent_dirs(const char *filepath);

void save_battery_to_file(gbmulator_t *emu, const char *path);

void load_battery_from_file(gbmulator_t *emu, const char *path);

int save_state_to_file(gbmulator_t *emu, const char *path, int compressed);

int load_state_from_file(gbmulator_t *emu, const char *path);

/**
 * @returns the contents of the file at `path` or `NULL` if the contents are invalid or an unsupported ROM.
 *          gb_init() won't fail if the return value is not `NULL`.
 */
uint8_t *get_rom(const char *path, size_t *rom_size);

char *get_xdg_path(const char *xdg_variable, const char *fallback);

char *get_config_path(void);

char *get_save_path(const char *rom_filepath);

char *get_savestate_path(const char *rom_filepath, int slot);

/**
 * resize, crop (keep center) and rotate `src` to fit into `dst` where `dst` is the gb camera sensor buffer
 */
void fit_image(uint8_t *dst, const uint8_t *src, int src_width, int src_height, int row_stride, int rotation);
