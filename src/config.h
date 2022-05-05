#pragma once

#include "emulator/emulator.h"

struct config {
    byte_t scale;
    float speed;
    float sound;
    char link_host[40];
    int link_port;

    SDL_Keycode left;
    SDL_Keycode right;
    SDL_Keycode up;
    SDL_Keycode down;
    SDL_Keycode a;
    SDL_Keycode b;
    SDL_Keycode start;
    SDL_Keycode select;
} extern config;

const char *config_load(void);

void config_save(const char* config_path);
