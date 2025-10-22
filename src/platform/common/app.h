#pragma once

#include "config.h"

void app_init(config_t *default_config);

void app_quit(void);

void app_reset(void);

void app_loop(void);

void app_joypad_press(unsigned int key, bool is_gamepad);

void app_joypad_release(unsigned int key, bool is_gamepad);

void app_load_cartridge(uint8_t *rom, size_t rom_size);

void app_set_pause(bool is_paused);

void app_set_sound(float value);

void app_set_mode(gbmulator_mode_t mode);

void app_set_speed(float value);

void app_set_palette(gb_color_palette_t value);

void app_get_screen_size(uint32_t *screen_w, uint32_t *screen_h);

void app_set_size(uint32_t viewport_w, uint32_t viewport_h);

void app_get_config(config_t *config);

bool app_is_paused(void);

void app_save_state(int slot);

void app_load_state(int slot);

uint32_t app_get_fps(void);
