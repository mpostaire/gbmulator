#pragma once

#include "types.h"
#include "config.h"

enum ppu_mode {
    PPU_HBLANK,
    PPU_VBLANK,
    PPU_OAM,
    PPU_DRAWING
};

#define PPU_IS_MODE(m) ((mem[STAT] & 0x03) == (m))

#define SET_PIXEL(buf, x, y, color) *(buf + ((y) * 160 * 3) + ((x) * 3)) = color_palettes[config.color_palette][(color)][0]; \
                            *(buf + ((y) * 160 * 3) + ((x) * 3) + 1) = color_palettes[config.color_palette][(color)][1]; \
                            *(buf + ((y) * 160 * 3) + ((x) * 3) + 2) = color_palettes[config.color_palette][(color)][2];

extern byte_t color_palettes[][4][3];

extern byte_t pixels[160 * 144 * 3];

/**
 * convert the pixels buffer from the color values of the old emulation palette to the new color values of the new palette
 */
void ppu_update_pixels_with_palette(byte_t palette);

byte_t ppu_step(int cycles);
