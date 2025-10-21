#pragma once

#include "config.h"

void app_init(config_t *default_config);

void app_quit(void);

void app_reset(void);

void app_loop(void);

void app_joypad_press(gbmulator_joypad_button_t button);

void app_joypad_release(gbmulator_joypad_button_t button);

void app_load_cartridge(uint8_t *rom, size_t rom_size);

void app_set_pause(bool is_paused);

void app_set_sound(float value);

void app_set_mode(gbmulator_mode_t mode);

void app_set_speed(float value);

void app_set_palette(gb_color_palette_t value);

void app_set_size(uint32_t screen_w, uint32_t screen_h, uint32_t viewport_w, uint32_t viewport_h);

void app_get_config(config_t *config);

gbmulator_joypad_button_t app_keycode_to_joypad(unsigned int keycode);

gbmulator_joypad_button_t app_button_to_joypad(unsigned int button);
