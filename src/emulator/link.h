#pragma once

#include "types.h"
#include "serialize.h"

void link_set_clock(emulator_t *emu);

void link_init(emulator_t *emu);

void link_quit(emulator_t *emu);

void link_step(emulator_t *emu);

SERIALIZE_FUNCTION_DECLS(link);
