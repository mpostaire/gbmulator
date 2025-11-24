#include <stdlib.h>

#include "bmp.h"
#include "../../core/types.h"

typedef struct
{
    uint16_t signature;
    uint32_t file_size;
    uint32_t offset;
} bmp_file_header_t;

typedef struct
{
    uint32_t header_size;
    uint32_t w;
    uint32_t h;
    uint32_t np;
    uint32_t depth;
    uint32_t compression;
    uint32_t img_size;
    uint32_t hres;
    uint32_t vres;
    uint32_t n_palette_colors;
    uint32_t n_important_palette_colors;
    uint32_t r_bitmask;
    uint32_t g_bitmask;
    uint32_t b_bitmask;
    uint32_t a_bitmask;
    uint32_t color_space_endpoints;
    uint32_t r_gamma;
    uint32_t g_gamma;
    uint32_t b_gamma;
    uint32_t intent;
    uint32_t icc_profile_data;
    uint32_t icc_profile_size;
} bmp_v5_header_t;

static inline uint8_t read_u8(uint8_t **data) {
    uint8_t ret = **data;
    *data += sizeof(ret);
    return ret;
}

static inline uint16_t read_u16(uint8_t **data) {
    return read_u8(data) | (((uint16_t) read_u8(data)) << 8);
}

static inline uint32_t read_u32(uint8_t **data) {
    return read_u16(data) | (((uint32_t) read_u16(data)) << 16);
}

static inline uint8_t get_shift_from_mask(uint32_t mask) {
    uint8_t shift = 0;

    while ((~mask) & 1) {
        mask >>= 1;
        shift++;
    }

    return shift;
}

static bool parse_file_header(uint8_t **data, bmp_file_header_t *file_header) {
    file_header->signature = read_u16(data);
    if (file_header->signature != 0x4D42)
        return false;

    file_header->file_size = read_u32(data);
    read_u32(data); // reserved
    file_header->offset = read_u32(data);

    return true;
}

static bool parse_v5_header(uint8_t **data, bmp_v5_header_t *v5_header) {
    v5_header->header_size = read_u32(data);
    v5_header->w           = read_u32(data);
    v5_header->h           = read_u32(data);

    v5_header->np = read_u16(data);
    if (v5_header->np != 1)
        return false;

    v5_header->depth = read_u16(data);
    if (v5_header->depth != 32)
        return false;

    v5_header->compression = read_u32(data);
    if (v5_header->compression != 3)
        return false;

    v5_header->img_size = read_u32(data);
    v5_header->hres     = read_u32(data);
    v5_header->vres     = read_u32(data);

    v5_header->n_palette_colors = read_u32(data);
    if (v5_header->n_palette_colors != 0)
        return false;

    v5_header->n_important_palette_colors = read_u32(data);
    if (v5_header->n_important_palette_colors != 0)
        return false;

    v5_header->r_bitmask = read_u32(data);
    v5_header->g_bitmask = read_u32(data);
    v5_header->b_bitmask = read_u32(data);
    v5_header->a_bitmask = read_u32(data);

    v5_header->color_space_endpoints = read_u32(data);
    v5_header->r_gamma               = read_u32(data);
    v5_header->g_gamma               = read_u32(data);
    v5_header->b_gamma               = read_u32(data);
    v5_header->intent                = read_u32(data);
    v5_header->icc_profile_data      = read_u32(data);

    v5_header->icc_profile_size = read_u32(data);
    if (v5_header->icc_profile_size != 0)
        return false;

    read_u32(data); // reserved

    return true;
}

bmp_image_t *bmp_decode(uint8_t *data, size_t size) {
    if (size < 50)
        return NULL;

    uint8_t          *data_ptr    = data;
    bmp_file_header_t file_header = {};
    bmp_v5_header_t   v5_header   = {};

    if (!parse_file_header(&data_ptr, &file_header))
        return NULL;

    if (!parse_v5_header(&data_ptr, &v5_header))
        return NULL;

    data_ptr = data + file_header.offset;

    uint32_t r_bitshift = get_shift_from_mask(v5_header.r_bitmask);
    uint32_t g_bitshift = get_shift_from_mask(v5_header.g_bitmask);
    uint32_t b_bitshift = get_shift_from_mask(v5_header.b_bitmask);
    uint32_t a_bitshift = get_shift_from_mask(v5_header.a_bitmask);

    bmp_image_t *img = malloc(sizeof(*img) + (v5_header.w * v5_header.h * (v5_header.depth / 4)));
    if (!img)
        return NULL;

    // bmp data rows are stored bottom to top and 0-padded at the end to the nearest 4-byte boundary
    uint8_t padding = (v5_header.w * (v5_header.depth / 4)) % 4;

    for (int32_t y = v5_header.h - 1; y >= 0; y--) {
        for (uint32_t x = 0; x < v5_header.w; x++) {
            uint32_t pixel = read_u32(&data_ptr);

            uint32_t offset = (y * (v5_header.w * 4)) + (x * 4);

            img->data[offset++] = (pixel & v5_header.r_bitmask) >> r_bitshift;
            img->data[offset++] = (pixel & v5_header.g_bitmask) >> g_bitshift;
            img->data[offset++] = (pixel & v5_header.b_bitmask) >> b_bitshift;
            img->data[offset]   = (pixel & v5_header.a_bitmask) >> a_bitshift;
        }

        for (uint8_t i = 0; i < padding; i++)
            read_u8(&data_ptr);
    }

    img->w = v5_header.w;
    img->h = v5_header.h;

    return img;
}
