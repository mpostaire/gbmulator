#pragma once

#include "types.h"

int emulator_save_state(emulator_t *emu, const char *path);

int emulator_load_state(emulator_t *emu, const char *path);

byte_t *emulator_get_state_data(emulator_t *emu, size_t *length);

int emulator_load_state_data(emulator_t *emu, const byte_t *data, size_t length);
