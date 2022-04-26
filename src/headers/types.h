#pragma once

#include <SDL2/SDL.h>

typedef unsigned char byte_t;
typedef char s_byte_t;
typedef unsigned short word_t;
typedef short s_word_t;

// TODO? LCD off special bright white color
typedef enum color {
    WHITE,
    LIGHT_GRAY,
    DARK_GRAY,
    BLACK
} color;

enum buttons {
    RIGHT = SDLK_RIGHT,
    LEFT = SDLK_LEFT,
    UP = SDLK_UP,
    DOWN = SDLK_DOWN,
    A = SDLK_KP_0,
    B = SDLK_KP_PERIOD,
    SELECT = SDLK_KP_2,
    START = SDLK_KP_1
};
