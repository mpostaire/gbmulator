#pragma once

#include "types.h"

void link_set_clock(emulator_t *emu);

void link_init(emulator_t *emu);

void link_quit(emulator_t *emu);

void link_step(emulator_t *emu);

size_t link_serialized_length(emulator_t *emu);

byte_t *link_serialize(emulator_t *emu, size_t *size);

void link_unserialize(emulator_t *emu, byte_t *buf);
