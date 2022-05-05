#pragma once

#include "types.h"

#define PPU_IS_MODE(m) ((mmu.mem[STAT] & 0x03) == (m))

#define GB_SCREEN_WIDTH 160
#define GB_SCREEN_HEIGHT 144

typedef enum {
    PPU_HBLANK,
    PPU_VBLANK,
    PPU_OAM,
    PPU_DRAWING
} ppu_mode;

typedef enum {
    PPU_COLOR_PALETTE_GRAY,
    PPU_COLOR_PALETTE_ORIG,
    PPU_COLOR_PALETTE_MAX // keep at the end
} ppu_color_palette;

typedef struct {
    void (*vblank_cb)(byte_t *pixels);
    byte_t current_color_palette;

    byte_t color_palettes[PPU_COLOR_PALETTE_MAX][4][3];

    byte_t pixels[GB_SCREEN_WIDTH * GB_SCREEN_HEIGHT * 3];
    byte_t pixels_cache_color_data[GB_SCREEN_WIDTH][GB_SCREEN_HEIGHT];

    int ppu_cycles;
} ppu_t;

extern ppu_t ppu;

/**
 * convert the pixels buffer from the color values of the old emulation palette to the new color values of the new palette
 */
void ppu_update_pixels_with_palette(byte_t palette);

void ppu_step(int cycles);

void ppu_init(void (*vblank_callback)(byte_t *));
