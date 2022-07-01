#pragma once

#include "types.h"

void link_init(emulator_t *emu);

void link_quit(emulator_t *emu);

int link_start_server(emulator_t *emu, const char *port);

int link_connect_to_server(emulator_t *emu, const char *address, const char *port);

void link_step(emulator_t *emu, int cycles);
