#pragma once

#include "gb.h"

typedef struct {
    uint16_t cycles;
    uint16_t max_clock_cycles;
    uint8_t bit_shift_counter;
} gb_link_t;

void link_set_clock(gb_t *gb);

void link_init(gb_t *gb);

void link_quit(gb_t *gb);

void link_step(gb_t *gb);
