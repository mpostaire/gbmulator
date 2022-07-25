#pragma once

#include <stddef.h>
#ifdef __ANDROID__
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif

#include "emulator/emulator.h"

struct config {
    emulator_mode_t mode;
    color_palette_t color_palette;
    byte_t scale;
    float speed;
    float sound;
    char link_host[INET6_ADDRSTRLEN];
    char link_port[6];
    byte_t is_ipv6;
    byte_t mptcp_enabled;

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

void config_load_from_buffer(const char *buf);

char *config_save_to_buffer(size_t *len);
