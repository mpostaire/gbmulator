#pragma once

#include "types.h"

void link_set_clock(emulator_t *emu);

void link_init(emulator_t *emu);

void link_quit(emulator_t *emu);

void link_step(emulator_t *emu, int cycles);
