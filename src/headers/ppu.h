#pragma once

#include "types.h"

void ppu_switch_colors(void);

void ppu_step(int cycles, SDL_Renderer *renderer, SDL_Texture *texture);
