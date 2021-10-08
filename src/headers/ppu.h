#pragma once

#include "types.h"

enum ppu_mode {
    PPU_HBLANK,
    PPU_VBLANK,
    PPU_OAM,
    PPU_DRAWING
};

#define PPU_IS_MODE(m) ((mem[STAT] & 0x03) == (m))

void ppu_switch_colors(void);

byte_t *ppu_step(int cycles);

byte_t *ppu_debug_oam();
