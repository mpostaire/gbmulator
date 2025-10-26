#pragma once

#include "gb.h"
#include "serialize.h"

typedef enum {
    TIMA_COUNTING,
    TIMA_LOADING,
    TIMA_LOADING_END,
    TIMA_LOADING_CANCELLED
} tima_state_t;

typedef struct {
    uint16_t div_timer;
    uint8_t  tima_increase_div_bit; // bit of DIV that causes an increase of TIMA when set to 1 and TAC timer is enabled
    uint8_t  tima_state;
    uint8_t  tima_cancelled_value;
    uint8_t  old_tima_signal; // used by the tima signal falling edge detector
} gb_timer_t;

void timer_set_div_timer(gb_t *gb, uint16_t value);

void timer_step(gb_t *gb);

void timer_reset(gb_t *gb);

SERIALIZE_FUNCTION_DECLS(timer);
