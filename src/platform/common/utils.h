#pragma once

#include "../../core/gb.h"
#include "../common/config.h"

int keycode_to_joypad(config_t *config, unsigned int keycode);

int button_to_joypad(config_t *config, unsigned int button);

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

void save_battery_to_file(gb_t *gb, const char *path);

void load_battery_from_file(gb_t *gb, const char *path);

int save_state_to_file(gb_t *gb, const char *path, int compressed);

int load_state_from_file(gb_t *gb, const char *path);
