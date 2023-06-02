#pragma once

#include "types.h"
#include "serialize.h"

void timer_step(emulator_t *emu);

void timer_init(emulator_t *emu);

void timer_quit(emulator_t *emu);

SERIALIZE_FUNCTION_DECLS(timer);
