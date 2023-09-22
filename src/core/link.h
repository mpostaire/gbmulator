#pragma once

#include "types.h"
#include "serialize.h"

typedef struct {
    word_t cycles;
    word_t max_clock_cycles;
    byte_t bit_counter;
    gb_t *other_emu;
    gb_printer_t *printer;
} gb_link_t;

void link_set_clock(gb_t *gb);

void link_init(gb_t *gb);

void link_quit(gb_t *gb);

void link_step(gb_t *gb);

SERIALIZE_FUNCTION_DECLS(link);
