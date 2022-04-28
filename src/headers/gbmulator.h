#pragma once

#include "types.h"

struct config {
    byte_t scale;
    float speed;
    float sound;
    byte_t color_palette;
    byte_t link_mode;
    char link_host[40];
    word_t link_port;
} extern config;

void gbmulator_exit(void);

void gbmulator_unpause(void);
