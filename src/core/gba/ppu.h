#pragma once

#include "gba.h"

typedef struct {
    uint32_t cycles;

    uint8_t pixels[GBA_SCREEN_WIDTH * GBA_SCREEN_HEIGHT * 4];
} gba_ppu_t;

void gba_ppu_init(gba_t *gba);

void gba_ppu_quit(gba_ppu_t *ppu);

void gba_ppu_step(gba_t *gba);
