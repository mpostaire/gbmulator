#pragma once

#include "gb.h"

typedef struct {
    uint8_t action;
    uint8_t direction;
} gb_joypad_t;

void joypad_init(gb_t *gb);

void joypad_quit(gb_t *gb);

uint8_t joypad_get_input(gb_t *gb);

void joypad_press(gb_t *gb, joypad_button_t key);

void joypad_release(gb_t *gb, joypad_button_t key);
