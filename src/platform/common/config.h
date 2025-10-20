#pragma once

#include <stddef.h>
#include <arpa/inet.h>

#include "../../core/core.h"

typedef bool (*keycode_filter_t)(unsigned int keycode);

typedef struct {
    gbmulator_mode_t   mode;
    gb_color_palette_t color_palette;
    float              speed;
    float              sound;
    float              joypad_opacity;
    uint8_t            sound_drc;
    uint8_t            enable_joypad;
    char               link_host[INET6_ADDRSTRLEN];
    char               link_port[6];

    unsigned int gamepad_bindings[10];

    unsigned int     keybindings[10];
    keycode_filter_t keycode_filter;
} config_t;

void config_load_from_string(config_t *config, const char *buf);

char *config_save_to_string(config_t *config);

void config_load_from_file(config_t *config, const char *path);

void config_save_to_file(config_t *config, const char *path);
