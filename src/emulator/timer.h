#pragma once

#include "types.h"

void timer_step(emulator_t *emu);

void timer_init(emulator_t *emu);

void timer_quit(emulator_t *emu);

size_t timer_serialized_length(emulator_t *emu);

byte_t *timer_serialize(emulator_t *emu, size_t *size);

void timer_unserialize(emulator_t *emu, byte_t *buf);
