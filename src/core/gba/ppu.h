#pragma once

#include "gba.h"

#define PPU_GET_MODE(gba) ((gba)->bus.io[IO_DISPCNT] & 0x07)

typedef enum {
    GBA_PPU_PERIOD_HDRAW,
    GBA_PPU_PERIOD_HBLANK,
    GBA_PPU_PERIOD_VBLANK
} gba_ppu_period_t;

typedef struct {
    uint64_t last_sync_cycle;

    uint32_t scanline_cycle;

    uint8_t obj_id;

    uint16_t line_layers[4][GBA_SCREEN_WIDTH];
    uint16_t obj_layers[2][GBA_SCREEN_WIDTH];

    uint8_t *pixels;
} gba_ppu_t;

void gba_ppu_enter_vhdraw(gba_t *gba);

void gba_ppu_enter_vhblank(gba_t *gba);

void gba_ppu_reset(gba_t *gba);

void gba_ppu_sync(gba_t *gba);
