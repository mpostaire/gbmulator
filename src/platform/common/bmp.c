#include <stdlib.h>

#include "bmp.h"
#include "../../core/utils.h"
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
    uint16_t np;
    uint16_t depth;
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

static inline uint8_t read_u8(uint8_t **buffer) {
    uint8_t ret  = **buffer;
    *buffer     += sizeof(ret);
    return ret;
}

static inline uint16_t read_u16(uint8_t **buffer) {
    return read_u8(buffer) | (((uint16_t) read_u8(buffer)) << 8);
}

static inline uint32_t read_u32(uint8_t **buffer) {
    return read_u16(buffer) | (((uint32_t) read_u16(buffer)) << 16);
}

static inline void write_u8(uint8_t **buffer, uint8_t data) {
    **buffer  = data;
    *buffer  += sizeof(data);
}

static inline void write_u16(uint8_t **buffer, uint16_t data) {
    write_u8(buffer, data);
    write_u8(buffer, data >> 8);
}

static inline void write_u32(uint8_t **buffer, uint32_t data) {
    write_u16(buffer, data);
    write_u16(buffer, data >> 16);
}

static inline uint8_t get_shift_from_mask(uint32_t mask) {
    uint8_t shift = 0;

    while ((~mask) & 1) {
        mask >>= 1;
        shift++;
    }

    return shift;
}

static bool read_file_header(uint8_t **buffer, bmp_file_header_t *file_header) {
    file_header->signature = read_u16(buffer);
    if (file_header->signature != 0x4D42)
        return false;

    file_header->file_size = read_u32(buffer);
    read_u32(buffer); // reserved
    file_header->offset = read_u32(buffer);

    return true;
}

static bool read_v5_header(uint8_t **buffer, bmp_v5_header_t *v5_header) {
    v5_header->header_size = read_u32(buffer);
    v5_header->w           = read_u32(buffer);
    v5_header->h           = read_u32(buffer);

    v5_header->np = read_u16(buffer);
    if (v5_header->np != 1)
        return false;

    v5_header->depth = read_u16(buffer);
    if (v5_header->depth != 32)
        return false;

    v5_header->compression = read_u32(buffer);
    if (v5_header->compression != 3)
        return false;

    v5_header->img_size = read_u32(buffer);
    v5_header->hres     = read_u32(buffer);
    v5_header->vres     = read_u32(buffer);

    v5_header->n_palette_colors = read_u32(buffer);
    if (v5_header->n_palette_colors != 0)
        return false;

    v5_header->n_important_palette_colors = read_u32(buffer);
    if (v5_header->n_important_palette_colors != 0)
        return false;

    v5_header->r_bitmask = read_u32(buffer);
    v5_header->g_bitmask = read_u32(buffer);
    v5_header->b_bitmask = read_u32(buffer);
    v5_header->a_bitmask = read_u32(buffer);

    v5_header->color_space_endpoints = read_u32(buffer);
    v5_header->r_gamma               = read_u32(buffer);
    v5_header->g_gamma               = read_u32(buffer);
    v5_header->b_gamma               = read_u32(buffer);
    v5_header->intent                = read_u32(buffer);
    v5_header->icc_profile_data      = read_u32(buffer);

    v5_header->icc_profile_size = read_u32(buffer);
    if (v5_header->icc_profile_size != 0)
        return false;

    read_u32(buffer); // reserved

    return true;
}

static bool write_file_header(uint8_t **buffer, bmp_file_header_t *file_header) {
    if (file_header->signature != 0x4D42)
        return false;

    write_u16(buffer, file_header->signature);

    write_u32(buffer, file_header->file_size);
    write_u32(buffer, 0); // reserved
    write_u32(buffer, file_header->offset);

    return true;
}

static bool write_v5_header(uint8_t **buffer, bmp_v5_header_t *v5_header) {
    if (v5_header->np != 1)
        return false;
    if (v5_header->depth != 32)
        return false;
    if (v5_header->compression != 3)
        return false;
    if (v5_header->n_palette_colors != 0)
        return false;
    if (v5_header->n_important_palette_colors != 0)
        return false;
    if (v5_header->icc_profile_size != 0)
        return false;

    write_u32(buffer, v5_header->header_size);
    write_u32(buffer, v5_header->w);
    write_u32(buffer, v5_header->h);
    write_u16(buffer, v5_header->np);

    write_u16(buffer, v5_header->depth);

    write_u32(buffer, v5_header->compression);

    write_u32(buffer, v5_header->img_size);
    write_u32(buffer, v5_header->hres);
    write_u32(buffer, v5_header->vres);

    write_u32(buffer, v5_header->n_palette_colors);

    write_u32(buffer, v5_header->n_important_palette_colors);

    write_u32(buffer, v5_header->r_bitmask);
    write_u32(buffer, v5_header->g_bitmask);
    write_u32(buffer, v5_header->b_bitmask);
    write_u32(buffer, v5_header->a_bitmask);

    write_u32(buffer, v5_header->color_space_endpoints);
    write_u32(buffer, v5_header->r_gamma);
    write_u32(buffer, v5_header->g_gamma);
    write_u32(buffer, v5_header->b_gamma);
    write_u32(buffer, v5_header->intent);
    write_u32(buffer, v5_header->icc_profile_data);

    write_u32(buffer, v5_header->icc_profile_size);

    write_u32(buffer, 0); // reserved

    return true;
}

bmp_image_t *bmp_decode(uint8_t *data, size_t size) {
    if (size < 50)
        return NULL;

    uint8_t          *data_ptr    = data;
    bmp_file_header_t file_header = {};
    bmp_v5_header_t   v5_header   = {};

    if (!read_file_header(&data_ptr, &file_header))
        return NULL;

    if (!read_v5_header(&data_ptr, &v5_header))
        return NULL;

    data_ptr = data + file_header.offset;

    uint32_t r_bitshift = get_shift_from_mask(v5_header.r_bitmask);
    uint32_t g_bitshift = get_shift_from_mask(v5_header.g_bitmask);
    uint32_t b_bitshift = get_shift_from_mask(v5_header.b_bitmask);
    uint32_t a_bitshift = get_shift_from_mask(v5_header.a_bitmask);

    bmp_image_t *img = xmalloc(sizeof(*img) + (v5_header.w * v5_header.h * (v5_header.depth / 4)));

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

uint8_t *bmp_encode(const bmp_image_t *img, size_t *out_size) {
    if (!img || !out_size)
        return NULL;

    uint32_t row_size        = img->w * 4;
    uint32_t padding         = (4 - (row_size % 4)) % 4;
    uint32_t pixel_data_size = (row_size + padding) * img->h;

    // FILE HEADER (14 bytes) + V5 HEADER (124 bytes)
    uint32_t header_size = 14 + 124;

    bmp_file_header_t file_header = {
        .signature = 0x4D42,
        .file_size = header_size + pixel_data_size,
        .offset    = header_size,
    };

    uint8_t *out = xmalloc(file_header.file_size);

    *out_size     = file_header.file_size;
    uint8_t *data = out;

    write_file_header(&data, &file_header);

    bmp_v5_header_t v5_header = {
        .header_size                = 124,
        .w                          = img->w,
        .h                          = img->h,
        .np                         = 1,
        .depth                      = 32,
        .compression                = 3,
        .img_size                   = pixel_data_size,
        .hres                       = 0,
        .vres                       = 0,
        .n_palette_colors           = 0,
        .n_important_palette_colors = 0,
        .r_bitmask                  = 0x00FF0000,
        .g_bitmask                  = 0x0000FF00,
        .b_bitmask                  = 0x000000FF,
        .a_bitmask                  = 0xFF000000,
        .color_space_endpoints      = 0,
        .r_gamma                    = 0,
        .g_gamma                    = 0,
        .b_gamma                    = 0,
        .intent                     = 0,
        .icc_profile_data           = 0,
        .icc_profile_size           = 0
    };

    write_v5_header(&data, &v5_header);

    for (int32_t y = v5_header.h - 1; y >= 0; y--) {
        for (uint32_t x = 0; x < v5_header.w; x++) {
            uint32_t offset = (y * (v5_header.w * 4)) + (x * 4);

            uint8_t r = img->data[offset++];
            uint8_t g = img->data[offset++];
            uint8_t b = img->data[offset++];
            uint8_t a = img->data[offset];

            write_u8(&data, b);
            write_u8(&data, g);
            write_u8(&data, r);
            write_u8(&data, a);
        }

        for (uint8_t i = 0; i < padding; i++)
            write_u8(&data, 0);
    }

    return out;
}
