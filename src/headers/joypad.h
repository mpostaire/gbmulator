#pragma once

#include "types.h"

extern byte_t joypad_action;
extern byte_t joypad_direction;

byte_t joypad_get_input(void);

void joypad_update(SDL_KeyboardEvent *key);
