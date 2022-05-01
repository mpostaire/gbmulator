#pragma once

#include "emulator/emulator.h"

struct config {
    byte_t scale;
    float speed;
    char link_host[40];
    int link_port;
} extern config;

const char *config_load(void);

void config_save(const char* config_path);
