#pragma once

#include <SDL2/SDL.h>

#include "emulator/emulator.h"

byte_t *ui_init(void);

void ui_back_to_main_menu(void);

void ui_draw_menu(void);

void ui_press(SDL_Keysym *keysym);

void ui_text_input(const char *text);

void ui_enable_resume_button(void);
