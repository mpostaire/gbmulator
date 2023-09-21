#pragma once

#include "types.h"
#include "serialize.h"

typedef struct {
    word_t cycles;
    word_t max_clock_cycles;
    byte_t bit_counter;
    emulator_t *other_emu;
    gb_printer_t *printer;
} link_t;

void link_set_clock(emulator_t *emu);

void link_init(emulator_t *emu);

void link_quit(emulator_t *emu);

void link_step(emulator_t *emu);

SERIALIZE_FUNCTION_DECLS(link);
