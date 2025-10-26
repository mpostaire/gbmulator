#pragma once

#include <SDL.h>

typedef struct {
    SDL_Rect shape;
    gbmulator_joypad_t button;
    SDL_Texture *texture;
} button_t;

void start_layout_editor(
    SDL_Renderer *renderer,
    SDL_bool is_landscape,
    int screen_width,
    int screen_height,
    SDL_Rect *gb_screen_rect,
    button_t *buttons);
