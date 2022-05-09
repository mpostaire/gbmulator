#pragma once

#include "types.h"

int emulator_save_state(const char *path);

int emulator_load_state(const char *path);

byte_t *emulator_get_state_data(size_t *length);

int emulator_load_state_data(const byte_t *data, size_t length);
