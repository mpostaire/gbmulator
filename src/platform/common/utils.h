#pragma once

#ifdef __ANDROID__
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif

int sdl_key_to_joypad(SDL_Keycode key);

int sdl_controller_to_joypad(SDL_GameControllerButton button);
