#pragma once

typedef struct {
    int div_counter;
    int tima_counter;
} gbtimer_t;

extern gbtimer_t timer;

void timer_step(int cycles);

void timer_init(void);
