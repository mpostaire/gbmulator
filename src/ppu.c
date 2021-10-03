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

byte_t (*color_values)[3] = color_gray;
byte_t change_colors = 0;

// TODO? LCD off special bright white color
enum color {
    WHITE,
    LIGHT_GRAY,
    DARK_GRAY,
    BLACK
};

byte_t pixels[160 * 144 * 3];

#define SET_PIXEL(x, y, c) pixels[((y) * 160 * 3) + ((x) * 3)] = color_values[(c)][0]; \
                            pixels[((y) * 160 * 3) + ((x) * 3) + 1] = color_values[(c)][1]; \
                            pixels[((y) * 160 * 3) + ((x) * 3) + 2] = color_values[(c)][2];

enum ppu_mode {
    PPU_HBLANK,
    PPU_VBLANK,
    PPU_OAM,
    PPU_DRAWING
};

#define IS_MODE(m) ((mem[STAT] & 0x03) == (m))
#define SET_MODE(m) mem[STAT] = (mem[STAT] & ~0x03) | (m)

int ppu_cycles = 0;

void switch_colors() {
    if (color_values == color_green)
        color_values = color_gray;
    else
        color_values = color_green;
    change_colors = 0;
}

void ppu_switch_colors(void) {
    change_colors = 1;
}

/**
 * @returns color after applying palette.
 */
static enum color get_color(byte_t color_data, word_t palette_address) {
    // get relevant bits of the color palette the color_data maps to
    byte_t hi, lo;
    switch (color_data) {
        case 0: hi = 1; lo = 0; break;
        case 1: hi = 3; lo = 2; break;
        case 2: hi = 5; lo = 4; break;
        case 3: hi = 7; lo = 6; break;
    }

    // get the color using the palette
    byte_t color = (GET_BIT(mem[palette_address], hi) << 1) | GET_BIT(mem[palette_address], lo);
    switch (color) {
        case 0: return WHITE;
        case 1: return LIGHT_GRAY;
        case 2: return DARK_GRAY;
        case 3: return BLACK;
    }

    // should never reach this
    return WHITE;
}

/**
 * Draws background and window pixels of line LY
 */
static void draw_bg_win(void) {
    byte_t y = mem[LY];
    byte_t window_y = mem[WY];
    byte_t window_x = mem[WX] - 7;
    byte_t scroll_x = mem[SCX];
    byte_t scroll_y = mem[SCY];

    word_t bg_tilemap_address = CHECK_BIT(mem[LCDC], 3) ? 0x9C00 : 0x9800;
    word_t win_tilemap_address = CHECK_BIT(mem[LCDC], 6) ? 0x9C00 : 0x9800;

    for (int x = 0; x < 160; x++) {
        // background and window disabled, draw white pixel
        if (!CHECK_BIT(mem[LCDC], 0)) {
            SET_PIXEL(x, y, WHITE);
            continue;
        }

        // 1. FIND TILE DATA

        // wich line on the background or window we are currently on
        byte_t pos_y;
        // wich row on the background or window we are currently on
        byte_t pos_x;

        // draw window if window enabled and current pixel inside window region
        byte_t win;
        if (CHECK_BIT(mem[LCDC], 5) && window_y <= y) {
            win = 1;
            pos_y = y - window_y;
            pos_x = x - window_x;
        } else {
            win = 0;
            pos_y = scroll_y + y;
            pos_x = scroll_x + x;
        }

        // find tile_id x,y coordinates in the memory "2D array" holding the tile_ids.
        // True y pos is SCY + LY. Then divide by 8 because tiles are 8x8 and multiply by 32 because this is a 32x32 "2D array" in a 1D array.
        word_t tile_id_y = (pos_y / 8) * 32;
        // True x pos is SCX + x. Then divide by 8 because tiles are 8x8.
        byte_t tile_id_x = pos_x / 8;
        word_t tile_id_address = (win ? win_tilemap_address : bg_tilemap_address) + tile_id_x + tile_id_y;

        // get tiledata memory address
        word_t tiledata_address;
        if (CHECK_BIT(mem[LCDC], 4)) {
            byte_t tile_id = mem[tile_id_address];
            tiledata_address = 0x8000 + tile_id * 16; // each tile takes 16 bytes in memory (8x8 pixels, 2 byte per pixels)
        } else {
            s_byte_t tile_id = (s_byte_t) mem[tile_id_address];
            tiledata_address = 0x8800 + tile_id * 16; // each tile takes 16 bytes in memory (8x8 pixels, 2 byte per pixels)
        }

        // find the vertical line of the tile we are on (% 8 because tiles are 8 pixels tall, * 2 because each line takes 2 bytes of memory)
        byte_t tiledata_line = (pos_y % 8) * 2;

        // 2. DRAW PIXEL

        // pixel color data takes 2 bytes in memory
        byte_t pixel_data_1 = mem[tiledata_address + tiledata_line];
        byte_t pixel_data_2 = mem[tiledata_address + tiledata_line + 1];

        // bit 7 of 1st byte == low bit of pixel 0, bit 7 of 2nd byte == high bit of pixel 0
        // bit 6 of 1st byte == low bit of pixel 1, bit 6 of 2nd byte == high bit of pixel 1
        // etc.
        byte_t relevant_bit = ((pos_x % 8) - 7) * -1;

        // construct color data
        byte_t color_data = (GET_BIT(pixel_data_2, relevant_bit) << 1) | GET_BIT(pixel_data_1, relevant_bit);
        // set pixel color using BG (for background and window) palette data
        SET_PIXEL(x, y, get_color(color_data, BGP));
    }
}

/**
 * Draws sprites of line LY
 */
static void draw_objects(void) {
    // objects disabled
    if (!CHECK_BIT(mem[LCDC], 1))
        return;
    
    byte_t y = mem[LY];
    byte_t rendered_objects = 0; // Keeps track of the number of rendered objects. There is a limit of 10 per scanline.

    // iterate over all possible object in OAM memory
    for (byte_t obj_id = 0; obj_id < 40; obj_id++) {
        // 1. GET OAM DATA

        // each object takes 4 bytes in the OAM
        word_t obj_address = OAM + (obj_id * 4);
        
        // obj y + 16 position is in byte 0
        byte_t pos_y = mem[obj_address] - 16;
        // obj x + 8 position is in byte 1
        word_t pos_x = mem[obj_address + 1] - 8;
        // obj position in the tile memory array
        byte_t tile_index = mem[obj_address + 2];
        // attributes flags
        byte_t flags = mem[obj_address + 3];

        // are objects 8x8 or 8x16?
        byte_t obj_height = 8;
        if (CHECK_BIT(flags, 2))
            obj_height = 16;

        // palette address
        word_t palette = OBP0;
        if (CHECK_BIT(flags, 4))
            palette = OBP1;

        // TODO object priority and bit 7 of obj attributes flags

        // 2. FIND OBJECT TILE DATA

        // don't render this object if it doesn't intercept the current scanline or there are already 10 objects on this scanline
        if (rendered_objects > 10 || y < pos_y || y >= pos_y + obj_height)
            continue;

        rendered_objects += 1;

        // find line of the object we are currently on
        byte_t obj_line = y - pos_y;
        if (CHECK_BIT(flags, 6)) // flip y
            obj_line = (obj_line - obj_height) * -1;
        obj_line *= 2; // each line takes 2 bytes in memory

        // find object tile data in memory
        word_t objdata_address = 0x8000 + tile_index * 16;

        // 3. DRAW OBJECT

        // pixel color data takes 2 bytes in memory
        byte_t pixel_data_1 = mem[objdata_address + obj_line];
        byte_t pixel_data_2 = mem[objdata_address + obj_line + 1];

        // bit 7 of 1st byte == low bit of pixel 0, bit 7 of 2nd byte == high bit of pixel 0
        // bit 6 of 1st byte == low bit of pixel 1, bit 6 of 2nd byte == high bit of pixel 1
        // etc.
        for (byte_t x = 0; x <= 7; x++) {
            byte_t relevant_bit = x; // flip x
            if (!CHECK_BIT(flags, 5)) // don't flip x
                relevant_bit = (relevant_bit - 7) * -1;
            
            // construct color data
            byte_t color_data = (GET_BIT(pixel_data_2, relevant_bit) << 1) | GET_BIT(pixel_data_1, relevant_bit);
            // get color using palette
            enum color c = get_color(color_data, palette);
            // set pixel color
            if (c != WHITE) { // WHITE is the transparent color for objects, don't draw it.
                SET_PIXEL(pos_x + x, y, c);
            }
        }
    }
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

            // if a change colors is requested, do it after rendering the current frame to avoid doing it mid frame
            if (change_colors)
                switch_colors();
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
                    draw_bg_win();
                    draw_objects();
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
