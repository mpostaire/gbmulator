#pragma once

#include "emulator/emulator.h"

struct config {
    byte_t scale;
    float speed;
    char link_host[40];
    int link_port;
} extern config;

const char *load_config(void);

void save_config(const char* config_path);
