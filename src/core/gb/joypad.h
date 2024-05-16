#pragma once

#include "types.h"

typedef struct {
    byte_t action;
    byte_t direction;
} gb_joypad_t;

void joypad_init(gb_t *gb);

void joypad_quit(gb_t *gb);

byte_t joypad_get_input(gb_t *gb);

void joypad_press(gb_t *gb, gb_joypad_button_t key);

void joypad_release(gb_t *gb, gb_joypad_button_t key);
