#pragma once

#include "gba.h"

typedef enum {
    GBA_PPU_PERIOD_HDRAW,
    GBA_PPU_PERIOD_HBLANK,
    GBA_PPU_PERIOD_VBLANK
} gba_ppu_period_t;

typedef struct {
    uint32_t pixel_cycles;
    gba_ppu_period_t period;

    uint32_t x;
    uint32_t y;

    uint8_t pixels[GBA_SCREEN_WIDTH * GBA_SCREEN_HEIGHT * 4];
} gba_ppu_t;

void gba_ppu_init(gba_t *gba);

void gba_ppu_quit(gba_ppu_t *ppu);

void gba_ppu_step(gba_t *gba);
