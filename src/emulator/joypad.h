#pragma once

#include "types.h"

typedef enum {
    JOYPAD_LEFT,
    JOYPAD_RIGHT,
    JOYPAD_UP,
    JOYPAD_DOWN,
    JOYPAD_A,
    JOYPAD_B,
    JOYPAD_START,
    JOYPAD_SELECT
} joypad_button_t;

void joypad_init(void);

byte_t joypad_get_input(void);

void joypad_press(joypad_button_t key);

void joypad_release(joypad_button_t key);
