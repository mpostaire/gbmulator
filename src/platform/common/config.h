#pragma once

#include <stddef.h>
#include <arpa/inet.h>

#include "emulator/emulator.h"

typedef int (*keycode_filter_t)(unsigned int keycode);
typedef const char *(*keycode_parser_t)(unsigned int keycode);
typedef unsigned int (*keyname_parser_t)(const char *keyname);

typedef struct {
    emulator_mode_t mode;
    color_palette_t color_palette;
    byte_t scale;
    float speed;
    float sound;
    char link_host[INET6_ADDRSTRLEN];
    char link_port[6];

    unsigned int gamepad_bindings[8];
    keycode_parser_t gamepad_button_parser;
    keyname_parser_t gamepad_button_name_parser;

    unsigned int keybindings[8];
    keycode_filter_t keycode_filter;
    keycode_parser_t keycode_parser;
    keyname_parser_t keyname_parser;
} config_t;

void config_load_from_string(config_t *config, const char *buf);

char *config_save_to_string(config_t *config);

void config_load_from_file(config_t *config, const char *path);

void config_save_to_file(config_t *config, const char *path);
