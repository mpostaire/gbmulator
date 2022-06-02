#pragma once

#include <SDL2/SDL.h>

#include "emulator/emulator.h"

extern emulator_t *emu;

void gbmulator_load_config(void);

void gbmulator_unpause(void);

void gbmulator_load_cartridge_from_data(void);

void gbmulator_reset(void);

int sdl_key_to_joypad(SDL_Keycode key);

int sdl_controller_to_joypad(int button);
