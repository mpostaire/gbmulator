#pragma once

#include "types.h"

enum joypad_button {
    JOYPAD_LEFT,
    JOYPAD_RIGHT,
    JOYPAD_UP,
    JOYPAD_DOWN,
    JOYPAD_A,
    JOYPAD_B,
    JOYPAD_START,
    JOYPAD_SELECT
};

byte_t joypad_get_input(void);

void joypad_press(enum joypad_button key);

void joypad_release(enum joypad_button key);
