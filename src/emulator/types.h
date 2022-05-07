#pragma once

#include <SDL2/SDL.h>

typedef unsigned char byte_t;
typedef char s_byte_t;
typedef unsigned short word_t;
typedef short s_word_t;

// TODO? LCD off special bright white color
typedef enum {
    WHITE,
    LIGHT_GRAY,
    DARK_GRAY,
    BLACK
} color_t;

typedef enum {
    PPU_COLOR_PALETTE_GRAY,
    PPU_COLOR_PALETTE_ORIG,
    PPU_COLOR_PALETTE_MAX // keep at the end
} color_palette_t;

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
