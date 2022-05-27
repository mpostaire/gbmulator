#pragma once

#include <SDL2/SDL.h>

#include "emulator/emulator.h"

extern emulator_t *emu;

void gbmulator_load_cartridge(const char *path);

void gbmulator_exit(void);

void gbmulator_unpause(void);

int sdl_key_to_joypad(SDL_Keycode key);

int sdl_controller_to_joypad(int button);
