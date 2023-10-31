#pragma once

#include "types.h"
#include "serialize.h"

typedef enum {
    TIMA_COUNTING,
    TIMA_LOADING,
    TIMA_LOADING_END,
    TIMA_LOADING_CANCELLED
} tima_state_t;

typedef struct {
    word_t div_timer;
    byte_t tima_increase_div_bit; // bit of DIV that causes an increase of TIMA when set to 1 and TAC timer is enabled
    byte_t tima_state;
    byte_t tima_cancelled_value;
    byte_t old_tima_signal; // used by the tima signal falling edge detector
} gb_timer_t;

void timer_set_div_timer(gb_t *gb, word_t value);

void timer_step(gb_t *gb);

void timer_init(gb_t *gb);

void timer_quit(gb_t *gb);

SERIALIZE_FUNCTION_DECLS(timer);
