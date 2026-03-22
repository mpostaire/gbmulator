#pragma once

#include <stdint.h>

typedef struct {
    uint32_t w;
    uint32_t h;
    uint8_t  data[];
} bmp_image_t;

/**
 * @brief Decodes a bmp image.
 * @param data Buffer of bmp data.
 * @param size Size of bmp data.
 * @return Pointer to decoded image. Caller is responsible to free it.
 */
bmp_image_t *bmp_decode(uint8_t *data, size_t size);

/**
 * @brief Encodes a bmp image.
 * @param img bmp image.
 * @param size Size of encoded data.
 * @return Pointer to encoded image.
 */
uint8_t *bmp_encode(const bmp_image_t *img, size_t *size);
