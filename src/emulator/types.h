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
} color;
