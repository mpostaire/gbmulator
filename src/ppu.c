#include <SDL2/SDL.h>

#include "ppu.h"
#include "types.h"
#include "mem.h"
#include "cpu.h"

// Grayscale colors
byte_t color_gray[4][3] = {
    { 0xFF, 0xFF, 0xFF },
    { 0xCC, 0xCC, 0xCC },
    { 0x77, 0x77, 0x77 },
    { 0x00, 0x00, 0x00 }
};
// Original colors
byte_t color_green[4][3] = {
    { 0x9B, 0xBC, 0x0F },
    { 0x8B, 0xAC, 0x0F },
    { 0x30, 0x62, 0x30 },
    { 0x0F, 0x38, 0x0F }
};
#define COLOR_VALUES color_green
// TODO? LCD off special bright white color
enum colors {
    WHITE,
    LIGH_GRAY,
    DARK_GRAY,
    BLACK
};
byte_t pixels[160 * 144 * 3];
#define SET_PIXEL(x, y, c) pixels[(y * 160 * 3) + (x * 3)] = COLOR_VALUES[c][0]; pixels[(y * 160 * 3) + (x * 3) + 1] = COLOR_VALUES[c][1]; pixels[(y * 160 * 3) + (x * 3) + 2] = COLOR_VALUES[c][2];

int ppu_cycles = 0;

enum ppu_modes {
    PPU_HBLANK,
    PPU_VBLANK,
    PPU_OAM,
    PPU_DRAWING
};
#define IS_MODE(m) ((mem[STAT] & 0x03) == m)
#define SET_MODE(m) mem[STAT] = (mem[STAT] & ~0x03) | m

/**
 * Draws background and window pixels of line LY
 */
static void ppu_draw_tiles() {
    // printf("%02x\n", mem[LCDC]);

    // TODO check if using byte_t type to automatically implement wrapping works (because of overflowing).

    byte_t y = mem[LY];
    byte_t window_y = mem[WY];
    byte_t window_x = mem[WX] - 7;
    byte_t scroll_x = mem[SCX];
    byte_t scroll_y = mem[SCY];

    word_t bg_tilemap_address = CHECK_BIT(mem[LCDC], 3) ? 0x9C00 : 0x9800;
    word_t win_tilemap_address = CHECK_BIT(mem[LCDC], 6) ? 0x9C00 : 0x9800;

    if (!CHECK_BIT(mem[LCDC], 0)) { // background and window disabled -- TODO not sure if this should clear all screen
        // TODO temporary red
        for (int x = 0; x < 160; x++) {
            pixels[(y * 160 * 3) + (x * 3)] = 0xFF;
            pixels[(y * 160 * 3) + (x * 3) + 1] = 0x00;
            pixels[(y * 160 * 3) + (x * 3) + 2] = 0x00;
        }
        return;
    }

    for (int x = 0; x < 160; x++) {

        // 1. FIND TILE DATA

        byte_t pos_y; // wich line on the background or window we are currently on
        byte_t win = 0;
        if (CHECK_BIT(mem[LCDC], 5) && y >= window_y && x >= window_x) { // draw window if window enabled and current pixel inside window region
            win = 1;
            pos_y = window_y - y;
        } else {
            pos_y = scroll_y + y;
        }

        // True y pos is SCY + LY. Then divide by 8 because tiles are 8x8 and multiply by 32 because this is a 32x32 "2D array" in a 1D array.
        byte_t tile_y = (pos_y / 8) * 32;
        // find the vertical line of the tile we are on (% 8 because tiles are 8 pixels tall, * 2 because each line takes 2 bytes of memory)
        byte_t tile_line = (tile_y % 8) * 2;

        byte_t pos_x = scroll_x + x;
        byte_t tile_x = pos_x / 8;

        // TODO tile actual location on memory is more complicated (signed/unsigned 0x8000......)
        word_t tiledata_address;
        int unsig = 1;
        if (CHECK_BIT(mem[LCDC], 4)) {
            tiledata_address = 0x8000;
        } else {
            tiledata_address = 0x8800;
            unsig = 0;
        }

        word_t tile_id_address = (win ? win_tilemap_address : bg_tilemap_address) + tile_x + tile_y;

        byte_t tile_id = mem[tile_id_address];

        // TODO understand this
        if (unsig) {
            tiledata_address += tile_id * 16;
        } else {
            tiledata_address += (tile_id + 128) * 16;
        }

        // 2. DRAW PIXEL

        // pixel color data takes 2 bytes in memory
        byte_t pixel_data_1 = mem[tiledata_address + tile_line];
        byte_t pixel_data_2 = mem[tiledata_address + tile_line + 1];

        // bit 7 of 1st byte == low bit of pixel 0, bit 7 of 2nd byte == high bit of pixel 0
        // bit 6 of 1st byte == low bit of pixel 1, bit 6 of 2nd byte == high bit of pixel 1
        // etc.
        byte_t relevant_bit = ((pos_x % 8) - 7) * -1;

        // construct color data
        byte_t color_data = (GET_BIT(pixel_data_2, relevant_bit) << 1) | GET_BIT(pixel_data_1, relevant_bit);
        // TODO palette
        SET_PIXEL(x, y, color_data);
    }
}

/**
 * Draws sprites of line LY
 */
static void ppu_draw_objects() {
    if (!CHECK_BIT(mem[LCDC], 1)) // objects disabled
        return;
}

/**
 * This does not implement the pixel FIFO but draws each scanline instantly when starting PPU_HBLANK (mode 0).
 * TODO if STAT interrupts are a problem, implement these corner cases: http://gameboy.mongenel.com/dmg/istat98.txt
 */
void ppu_step(int cycles, SDL_Renderer *renderer, SDL_Texture *texture) {
    ppu_cycles += cycles;

    if (!CHECK_BIT(mem[LCDC], 7)) { // is LCD disabled?
        // TODO not sure of the handling of LCD disabled
        // TODO LCD disabled should fill screen with a color brighter than WHITE
        ppu_cycles = 0;
        mem[LY] = 0;
        return;
    }

    if (mem[LY] == mem[LYC] && CHECK_BIT(mem[STAT], 6)) { // LY == LYC compare
        SET_BIT(mem[STAT], 2);
        cpu_request_interrupt(IRQ_STAT);
    } else {
        RESET_BIT(mem[STAT], 2);
    }

    if (mem[LY] == 144) { // Mode 1 (VBlank)
        if (!IS_MODE(PPU_VBLANK)) {
            SET_MODE(PPU_VBLANK);
            if (CHECK_BIT(mem[STAT], 4))
                cpu_request_interrupt(IRQ_STAT);
            cpu_request_interrupt(IRQ_VBLANK);
            
            // the frame is complete, it can be rendered
            SDL_UpdateTexture(texture, NULL, pixels, 160 * sizeof(byte_t) * 3);
            SDL_RenderCopy(renderer, texture, NULL, NULL);
            SDL_RenderPresent(renderer);
        }
    } else {
        if (ppu_cycles <= 80) { // Mode 2 (OAM) -- ends after 80 ppu_cycles
            if (!IS_MODE(PPU_OAM))
                SET_MODE(PPU_OAM);
                if (CHECK_BIT(mem[STAT], 5))
                    cpu_request_interrupt(IRQ_STAT);
        } else if (ppu_cycles <= 252) { // Mode 3 (Drawing) -- ends after 252 ppu_cycles
            if (!IS_MODE(PPU_DRAWING))
                SET_MODE(PPU_DRAWING);
        } else if (ppu_cycles <= 456) { // Mode 0 (HBlank) -- ends after 456 ppu_cycles
            if (!IS_MODE(PPU_HBLANK)) {
                SET_MODE(PPU_HBLANK);
                if (CHECK_BIT(mem[STAT], 3))
                    cpu_request_interrupt(IRQ_STAT);
                
                // draw scanline (background, window and objects pixels on line LY) when we enter HBlank
                if (mem[LY] < 144) {
                    ppu_draw_tiles();
                    ppu_draw_objects();
                }
            }
        }
    }

    if (ppu_cycles >= 456) {
        ppu_cycles -= 456; // not reset to 0 because there may be leftover cycles we want to take into account for the next scanline
        if (++mem[LY] > 153)
            mem[LY] = 0;
    }
}
