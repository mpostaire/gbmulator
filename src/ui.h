#pragma once

#include <SDL2/SDL.h>

#include "emulator/emulator.h"

byte_t *ui_init(void);

void ui_back_to_main_menu(void);

void ui_draw_menu(void);

void ui_press_joypad(joypad_button_t key);

void ui_keyboard_press(SDL_KeyboardEvent *keyevent);

void ui_controller_press(int button);

void ui_text_input(const char *text);

void ui_enable_resume_button(void);

void ui_enable_link_button(void);

void ui_enable_reset_button(void);
