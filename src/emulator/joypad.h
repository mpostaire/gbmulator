#pragma once

#include "types.h"

void joypad_init(emulator_t *emu);

void joypad_quit(emulator_t *emu);

byte_t joypad_get_input(emulator_t *emu);

void joypad_press(emulator_t *emu, joypad_button_t key);

void joypad_release(emulator_t *emu, joypad_button_t key);
