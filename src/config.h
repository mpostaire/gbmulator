#pragma once

#include <SDL2/SDL.h>

#include "emulator/emulator.h"

struct config {
    emulator_mode_t mode;
    color_palette_t color_palette;
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

int config_verif_key(SDL_Keycode key);

void config_load(const char* config_path);

void config_save(const char* config_path);
