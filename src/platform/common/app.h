#pragma once

#include "config.h"

typedef void (*printer_new_line_cb_t)(size_t current_height, size_t total_height);

void app_init(void);

// can be called before app_init() --> maybe rename app_init()
void app_load_config(const config_t *default_config);

void app_quit(void);

void app_reset(void);

void app_run_frame(void);

void app_render(void);

void app_keyboard_press(unsigned int key);

void app_keyboard_release(unsigned int key);

void app_gamepad_press(unsigned int button);

void app_gamepad_release(unsigned int button);

void app_touch_press(uint8_t touch_id, uint32_t x, uint32_t y);

void app_touch_release(uint8_t touch_id, uint32_t x, uint32_t y);

void app_touch_move(uint8_t touch_id, uint32_t x, uint32_t y);

bool app_load_cartridge(uint8_t *rom, size_t rom_size);

void app_set_pause(bool is_paused);

void app_set_sound(float value);

void app_set_drc(bool is_enabled);

gbmulator_mode_t app_get_mode(void);

bool app_set_mode(gbmulator_mode_t mode);

void app_set_speed(float value);

void app_set_palette(gb_color_palette_t value);

void app_get_screen_size(uint32_t *screen_w, uint32_t *screen_h);

void app_set_size(uint32_t viewport_w, uint32_t viewport_h);

void app_get_config(config_t *config);

bool app_is_paused(void);

void app_save_state(int slot);

void app_load_state(int slot);

uint32_t app_get_fps(void);

const char *app_get_rom_title(void);

void app_set_touchscreen_mode(bool enable);

void app_set_joypad_opacity(float value);

bool app_set_binding(bool is_gamepad, gbmulator_joypad_t joypad, unsigned int key, gbmulator_joypad_t *swapped_joypad, unsigned int *swapped_key);

bool app_connect_printer(printer_new_line_cb_t on_new_line);

void app_printer_disconnect(void);

void app_printer_render(void);

bool app_printer_reset(void);

bool app_has_camera(void);

void app_link_set_host(const char *host);

void app_link_set_port(uint16_t port);

bool app_link_start(bool is_server);

void app_link_disconnect(void);
