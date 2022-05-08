#pragma once

#include <SDL2/SDL.h>

void gbmulator_exit(void);

void gbmulator_unpause(void);

int sdl_key_to_joypad(SDL_Keycode key);

#ifdef __EMSCRIPTEN__
void gbmulator_request_config_save(void);
#else
void gbmulator_load_cartridge(const char *path);
#endif
