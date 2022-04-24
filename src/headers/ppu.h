#pragma once

#include "types.h"

enum ppu_mode {
    PPU_HBLANK,
    PPU_VBLANK,
    PPU_OAM,
    PPU_DRAWING
};

#define PPU_IS_MODE(m) ((mem[STAT] & 0x03) == (m))

#define SET_PIXEL(buf, x, y, c) *(buf + ((y) * 160 * 3) + ((x) * 3)) = color_values[(c)][0]; \
                            *(buf + ((y) * 160 * 3) + ((x) * 3) + 1) = color_values[(c)][1]; \
                            *(buf + ((y) * 160 * 3) + ((x) * 3) + 2) = color_values[(c)][2];

extern byte_t (*color_values)[3];

extern byte_t pixels[160 * 144 * 3];

void ppu_switch_colors(void);

byte_t ppu_step(int cycles);
