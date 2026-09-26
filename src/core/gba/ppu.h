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

    struct {
        uint16_t a;
        uint16_t b;
    } composite;

    struct {
        uint16_t sbe;
        uint8_t  scanline[GBA_SCREEN_WIDTH];
        int32_t  x;
    } bgs[4];

    struct {
        uint8_t id;
        uint8_t scanline_layers[2][GBA_SCREEN_WIDTH];
    } obj;

    uint8_t *pixels;
} gba_ppu_t;

void gba_ppu_enter_vhdraw(gba_t *gba);

void gba_ppu_enter_vhblank(gba_t *gba);

void gba_ppu_reset(gba_t *gba);

void gba_ppu_sync(gba_t *gba);
