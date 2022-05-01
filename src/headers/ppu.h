#pragma once

#include "types.h"

#define PPU_IS_MODE(m) ((mem[STAT] & 0x03) == (m))

enum ppu_mode {
    PPU_HBLANK,
    PPU_VBLANK,
    PPU_OAM,
    PPU_DRAWING
};

enum ppu_color_palette {
    PPU_COLOR_PALETTE_GRAY,
    PPU_COLOR_PALETTE_ORIG
};

/**
 * convert the pixels buffer from the color values of the old emulation palette to the new color values of the new palette
 */
void ppu_update_pixels_with_palette(byte_t palette);

void ppu_step(int cycles);

byte_t *ppu_get_pixels(void);

byte_t *ppu_get_color_values(enum color color);

void ppu_set_color_palette(enum ppu_color_palette palette);

byte_t ppu_get_color_palette(void);

void ppu_set_vblank_callback(void (*vblank_callback)(byte_t *));
