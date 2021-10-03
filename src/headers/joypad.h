#pragma once

#include <SDL2/SDL.h>

#include "types.h"

byte_t joypad_get_input(void);

void joypad_press(SDL_Keycode key);

void joypad_release(SDL_Keycode key);
