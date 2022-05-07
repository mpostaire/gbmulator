#pragma once

#include "types.h"

void joypad_init(void);

byte_t joypad_get_input(void);

void joypad_press(joypad_button_t key);

void joypad_release(joypad_button_t key);
