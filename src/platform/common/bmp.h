#pragma once

#include <stdint.h>

typedef struct {
    uint32_t w;
    uint32_t h;
    uint8_t *data;
} bmp_image_t;

bmp_image_t *bmp_decode(uint8_t *data, size_t size);
