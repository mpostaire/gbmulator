#pragma once

#include <SDL2/SDL.h>

#include "types.h"

enum ppu_mode {
    PPU_HBLANK,
    PPU_VBLANK,
    PPU_OAM,
    PPU_DRAWING
};

#define PPU_IS_MODE(m) ((mem[STAT] & 0x03) == (m))

void ppu_switch_colors(void);

void ppu_step(int cycles, SDL_Renderer *renderer, SDL_Texture *texture);
