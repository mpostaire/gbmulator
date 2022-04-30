#pragma once

#include "types.h"

struct config {
    byte_t scale;
    float speed;
    float sound;
    byte_t color_palette;
    char link_host[40];
    int link_port;
} extern config;

const char *load_config(void);

void save_config(const char* config_path);
