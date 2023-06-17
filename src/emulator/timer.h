#pragma once

#include "types.h"
#include "serialize.h"

typedef struct {
    byte_t tima_increase_div_bit; // bit of DIV that causes an increase of TIMA when set to 1 and TAC timer is enabled
    byte_t falling_edge_detector_delay;
    s_word_t tima_loading_value;
    s_word_t old_tma; // holds the value of the old tma for one cpu step if it has been overwritten, -1 otherwise
} gbtimer_t;

void timer_step(emulator_t *emu);

void timer_init(emulator_t *emu);

void timer_quit(emulator_t *emu);

SERIALIZE_FUNCTION_DECLS(timer);
