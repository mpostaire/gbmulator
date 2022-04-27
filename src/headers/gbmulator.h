#pragma once

#include "types.h"

struct config {
    byte_t scale;
    float speed;
    byte_t sound;
    byte_t color_palette;
    char *link_host;
    word_t link_port;
} extern config;

void gbmulator_exit(void);

void gbmulator_unpause(void);
