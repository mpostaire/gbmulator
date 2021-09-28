#pragma once

#include "types.h"

/**
 * @returns a pixel buffer pointer to render on screen only when VBlank is reached. NULL in any other PPU mode.
 */
void ppu_step(int cycles, SDL_Renderer *renderer, SDL_Texture *texture);
