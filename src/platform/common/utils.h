#pragma once

#include <SDL2/SDL.h>

int sdl_key_to_joypad(SDL_Keycode key);

int sdl_controller_to_joypad(SDL_GameControllerButton button);
