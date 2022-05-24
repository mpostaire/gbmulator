#pragma once

#include "emulator.h"
#include "types.h"

#define PPU_IS_MODE(m) ((mmu.mem[STAT] & 0x03) == (m))

typedef enum {
    PPU_MODE_HBLANK,
    PPU_MODE_VBLANK,
    PPU_MODE_OAM,
    PPU_MODE_DRAWING
} ppu_mode_t;

typedef struct {
    void (*new_frame_cb)(byte_t *pixels);
    byte_t current_color_palette;

    byte_t pixels[GB_SCREEN_WIDTH * GB_SCREEN_HEIGHT * 3];
    byte_t pixels_cache_color_data[GB_SCREEN_WIDTH][GB_SCREEN_HEIGHT];

    byte_t blank_pixels[GB_SCREEN_WIDTH * GB_SCREEN_HEIGHT * 3];

    int cycles;
} ppu_t;

extern byte_t ppu_color_palettes[PPU_COLOR_PALETTE_MAX][4][3];

extern ppu_t ppu;

void ppu_step(int cycles);

void ppu_init(void (*new_frame_cb)(byte_t *pixels));
