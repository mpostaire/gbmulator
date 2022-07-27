#pragma once

#ifdef __ANDROID__
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif

#include "emulator/emulator.h"

int sdl_key_to_joypad(SDL_Keycode key);

int sdl_controller_to_joypad(SDL_GameControllerButton button);

int dir_exists(const char *directory_path);

void mkdirp(const char *directory_path);

void make_parent_dirs(const char *filepath);

void save_battery_to_file(emulator_t *emu, const char *path);

void load_battery_from_file(emulator_t *emu, const char *path);

int save_state_to_file(emulator_t *emu, const char *path);

int load_state_from_file(emulator_t *emu, const char *path);
