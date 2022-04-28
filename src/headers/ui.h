#pragma once

#include <SDL2/SDL.h>

#include "types.h"

extern byte_t ui_pixels[160 * 144 * 4];

void ui_init(void);

void ui_back_to_main_menu(void);

void ui_draw_menu(void);

void ui_press(SDL_Keysym *keysym);

void ui_text_input(char *text);
