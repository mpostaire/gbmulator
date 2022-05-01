#pragma once

#include <SDL2/SDL.h>

void gbmulator_exit(void);

void gbmulator_unpause(void);

int sdl_key_to_joypad(SDL_Keycode key);
