#include <string.h>
#include <stdlib.h>

#include "emulator.h"
#include "ppu.h"
#include "mmu.h"
#include "cpu.h"

#define PPU_SET_MODE(m) mmu->mem[STAT] = (mmu->mem[STAT] & 0xFC) | (m)

#define SET_PIXEL_DMG(ppu_ptr, x, y, color) { *((ppu_ptr)->pixels + ((y) * GB_SCREEN_WIDTH * 3) + ((x) * 3)) = ppu_color_palettes[(ppu_ptr)->current_color_palette][(color)][0]; \
                            *((ppu_ptr)->pixels + ((y) * GB_SCREEN_WIDTH * 3) + ((x) * 3) + 1) = ppu_color_palettes[(ppu_ptr)->current_color_palette][(color)][1]; \
                            *((ppu_ptr)->pixels + ((y) * GB_SCREEN_WIDTH * 3) + ((x) * 3) + 2) = ppu_color_palettes[(ppu_ptr)->current_color_palette][(color)][2]; }

#define SET_PIXEL_CGB(ppu_ptr, x, y, r, g, b) { *((ppu_ptr)->pixels + ((y) * GB_SCREEN_WIDTH * 3) + ((x) * 3)) = (r); \
                            *((ppu_ptr)->pixels + ((y) * GB_SCREEN_WIDTH * 3) + ((x) * 3) + 1) = (g); \
                            *((ppu_ptr)->pixels + ((y) * GB_SCREEN_WIDTH * 3) + ((x) * 3) + 2) = (b); }

byte_t ppu_color_palettes[PPU_COLOR_PALETTE_MAX][4][3] = {
    { // grayscale colors
        { 0xFF, 0xFF, 0xFF },
        { 0xCC, 0xCC, 0xCC },
        { 0x77, 0x77, 0x77 },
        { 0x00, 0x00, 0x00 }
    },
    { // green colors (original)
        { 0x9B, 0xBC, 0x0F },
        { 0x8B, 0xAC, 0x0F },
        { 0x30, 0x62, 0x30 },
        { 0x0F, 0x38, 0x0F }
    }
};

extern inline void ppu_ly_lyc_compare(emulator_t *emu);

/**
 * @returns dmg color after applying palette.
 */
static inline dmg_color_t get_color_dmg(mmu_t *mmu, byte_t color_id, word_t palette_address) {
    // get relevant bits of the color palette the color_id maps to
    byte_t filter = 0; // initialize to 0 to shut gcc warnings
    switch (color_id) {
        case 0: filter = 0x03; break;
        case 1: filter = 0x0C; break;
        case 2: filter = 0x30; break;
        case 3: filter = 0xC0; break;
    }

    // return the color using the palette
    return (mmu->mem[palette_address] & filter) >> (color_id * 2);
}

/**
 * @returns cgb colors in r, g, b arguments after applying palette.
 */
static inline void get_color_cgb(word_t color_palette_data, byte_t *r, byte_t *g, byte_t *b) {
    *r = color_palette_data & 0x1F;
    *g = (color_palette_data & 0x3E0) >> 5;
    *b = (color_palette_data & 0x7C00) >> 10;

    // TODO instead of dmg palettes choice in ui, in cgb do color correction choice (from orig, gray to raw, dim(or fix))
    // r = (r << 3) | (r >> 2);
    // g = (g << 3) | (g >> 2);
    // b = (b << 3) | (b >> 2);

    // color correction
    int R = (26 * *r) + (4 * *g) + (2 * *b);
    int G = (24 * *g) + (8 * *b);
    int B = (6 * *r) + (4 * *g) + (22 * *b);

    *r = MIN(960, R) >> 2;
    *g = MIN(960, G) >> 2;
    *b = MIN(960, B) >> 2;
}

/**
 * Draws background and window pixels of line LY
 */
static void draw_bg_win(emulator_t *emu) {
    ppu_t *ppu = emu->ppu;
    mmu_t *mmu = emu->mmu;

    byte_t y = mmu->mem[LY];
    byte_t window_y = mmu->mem[WY];
    s_word_t window_x = mmu->mem[WX] - 7;
    byte_t scroll_x = mmu->mem[SCX];
    byte_t scroll_y = mmu->mem[SCY];

    byte_t win_in_scanline = 0;

    for (int x = 0; x < GB_SCREEN_WIDTH; x++) {
        // background and window disabled, draw white pixel
        // TODO this may be not the cause but dmg-acid2 test fails at the exclamation marks
        //      it's displayed only for one frame and after the bg/win enable bit (LCDC.0) is 0 and it doesn't show anymore
        //      it's as if the LCDC.0 reset happens 8 scanlines too soon (1 tile height) as it should be 1 at scanline 0 and 0 at scanline 8
        //      see https://i.imgur.com/x2R66WQ.png for an image showing the lines at which the LYC=LY STAT interrupt should fire
        if (!CHECK_BIT(mmu->mem[LCDC], 0)) {
            SET_PIXEL_DMG(ppu, x, y, get_color_dmg(mmu, DMG_WHITE, BGP));
            continue;
        }

        // 1. FIND TILE DATA

        // wich line on the background or window we are currently on
        byte_t pos_y;
        // wich row on the background or window we are currently on
        byte_t pos_x;

        word_t tilemap_address;

        // draw window if window enabled and current pixel inside window region
        if (CHECK_BIT(mmu->mem[LCDC], 5) && window_y <= y && x >= window_x) {
            win_in_scanline = 1;
            tilemap_address = CHECK_BIT(mmu->mem[LCDC], 6) ? 0x9C00 : 0x9800;
            pos_y = ppu->wly;
            pos_x = x - window_x;
        } else {
            tilemap_address = CHECK_BIT(mmu->mem[LCDC], 3) ? 0x9C00 : 0x9800;
            pos_y = scroll_y + y;
            pos_x = scroll_x + x;
        }

        // find tile_id x,y coordinates in the memory "2D array" holding the tile_ids.
        // True y pos is SCY + LY. Then divide by 8 because tiles are 8x8 and multiply by 32 because this is a 32x32 "2D array" in a 1D array.
        word_t tile_id_y = (pos_y / 8) * 32;
        // True x pos is SCX + x. Then divide by 8 because tiles are 8x8.
        byte_t tile_id_x = pos_x / 8;
        word_t tile_id_address = tilemap_address + tile_id_x + tile_id_y;

        // get tiledata memory address
        word_t tiledata_address;
        s_word_t tile_id;
        if (CHECK_BIT(mmu->mem[LCDC], 4)) {
            tile_id = mmu->mem[tile_id_address];
            tiledata_address = VRAM + tile_id * 16; // each tile takes 16 bytes in memory (8x8 pixels, 2 byte per pixels)
        } else {
            tile_id = (s_byte_t) mmu->mem[tile_id_address] + 128;
            tiledata_address = 0x8800 + tile_id * 16; // each tile takes 16 bytes in memory (8x8 pixels, 2 byte per pixels)
        }

        // find the vertical line of the tile we are on (% 8 because tiles are 8 pixels tall, * 2 because each line takes 2 bytes of memory)
        byte_t tiledata_line = (pos_y % 8) * 2;

        // 2. DRAW PIXEL

        // pixel color data takes 2 bytes in memory
        byte_t pixel_data_1 = mmu->mem[tiledata_address + tiledata_line];
        byte_t pixel_data_2 = mmu->mem[tiledata_address + tiledata_line + 1];

        // bit 7 of 1st byte == low bit of pixel 0, bit 7 of 2nd byte == high bit of pixel 0
        // bit 6 of 1st byte == low bit of pixel 1, bit 6 of 2nd byte == high bit of pixel 1
        // etc.
        byte_t relevant_bit = ((pos_x % 8) - 7) * -1;

        // construct color data
        byte_t color_id = (GET_BIT(pixel_data_2, relevant_bit) << 1) | GET_BIT(pixel_data_1, relevant_bit);
        // cache color_id to be used for objects rendering
        ppu->scanline_cache_color_data[x] = color_id;
        // set pixel color using BG (for background and window) palette data
        SET_PIXEL_DMG(ppu, x, y, get_color_dmg(mmu, color_id, BGP));
    }

    // increment ppu->wly if the window is present in this scanline
    if (win_in_scanline)
        ppu->wly++;
}

/**
 * Draws sprites of line LY
 */
static void draw_objects(emulator_t *emu) {
    ppu_t *ppu = emu->ppu;
    mmu_t *mmu = emu->mmu;

    // objects disabled
    if (!CHECK_BIT(mmu->mem[LCDC], 1))
        return;

    // are objects 8x8 or 8x16?
    byte_t obj_height = 8;
    if (CHECK_BIT(mmu->mem[LCDC], 2))
        obj_height = 16;

    // store the x position of each pixel drawn by the highest priority object
    byte_t obj_pixel_priority[GB_SCREEN_WIDTH];
    // default at 255 because no object will have that much of x coordinate (and if so, will not be visible anyway)
    memset(obj_pixel_priority, 255, sizeof(obj_pixel_priority));

    byte_t y = mmu->mem[LY];

    // iterate over all objects to render
    for (short i = ppu->oam_scan.size - 1; i >= 0; i--) {
        word_t obj_address = ppu->oam_scan.objs_addresses[i];
        // obj y + 16 position is in byte 0
        s_word_t pos_y = mmu->mem[obj_address] - 16;
        // obj x + 8 position is in byte 1 (signed word important here!)
        s_word_t pos_x = mmu->mem[obj_address + 1] - 8;
        // obj position in the tile memory array
        byte_t tile_index = mmu->mem[obj_address + 2];
        if (obj_height > 8)
            tile_index &= 0xFE;
        // attributes flags
        byte_t flags = mmu->mem[obj_address + 3];

        // palette address
        word_t palette = OBP0;
        if (CHECK_BIT(flags, 4))
            palette = OBP1;

        // find line of the object we are currently on
        byte_t obj_line = y - pos_y;
        if (CHECK_BIT(flags, 6)) // flip y
            obj_line = (obj_line - (obj_height - 1)) * -1;
        obj_line *= 2; // each line takes 2 bytes in memory

        // find object tile data in memory
        word_t objdata_address = VRAM + tile_index * 16;

        // 3. DRAW OBJECT

        // pixel color data takes 2 bytes in memory
        byte_t pixel_data_1 = mmu->mem[objdata_address + obj_line];
        byte_t pixel_data_2 = mmu->mem[objdata_address + obj_line + 1];

        // bit 7 of 1st byte == low bit of pixel 0, bit 7 of 2nd byte == high bit of pixel 0
        // bit 6 of 1st byte == low bit of pixel 1, bit 6 of 2nd byte == high bit of pixel 1
        // etc.
        for (byte_t x = 0; x <= 7; x++) {
            s_word_t pixel_x = pos_x + x;
            // don't draw pixel if it's outside the screen
            if (pixel_x < 0 || pixel_x > 159)
                continue;

            byte_t relevant_bit = x; // flip x
            if (!CHECK_BIT(flags, 5)) // don't flip x
                relevant_bit = (relevant_bit - 7) * -1;

            // construct color data
            byte_t color_id = (GET_BIT(pixel_data_2, relevant_bit) << 1) | GET_BIT(pixel_data_1, relevant_bit);
            if (color_id == DMG_WHITE) // DMG_WHITE is the transparent color for objects, don't draw it (this needs to be done BEFORE palette conversion).
                continue;

            // if an object with lower x coordinate has already drawn this pixel, it has higher priority so we abort
            if (obj_pixel_priority[pixel_x] < pos_x)
                continue;

            // store current object x coordinate in priority array
            obj_pixel_priority[pixel_x] = pos_x;

            // if object is below background, object can only be drawn current if background/window color (before applying palette) is 0
            if (CHECK_BIT(flags, 7) && ppu->scanline_cache_color_data[pixel_x])
                continue;

            // set pixel color using palette
            SET_PIXEL_DMG(ppu, pixel_x, y, get_color_dmg(mmu, color_id, palette));
        }
    }
}

/**
 * Draws background and window pixels of line LY
 */
static void draw_bg_win_cgb(emulator_t *emu) {
    ppu_t *ppu = emu->ppu;
    mmu_t *mmu = emu->mmu;

    byte_t y = mmu->mem[LY];
    byte_t window_y = mmu->mem[WY];
    s_word_t window_x = mmu->mem[WX] - 7;
    byte_t scroll_x = mmu->mem[SCX];
    byte_t scroll_y = mmu->mem[SCY];

    byte_t win_in_scanline = 0;

    for (int x = 0; x < GB_SCREEN_WIDTH; x++) {
        // TODO in dmg compatibility mode (use KEY0 register to check compat mode), set white pixel if lcdc.0 is 0

        // 1. FIND TILE DATA

        // wich line on the background or window we are currently on
        byte_t pos_y;
        // wich row on the background or window we are currently on
        byte_t pos_x;

        word_t tilemap_address;

        // draw window if window enabled and current pixel inside window region
        if (CHECK_BIT(mmu->mem[LCDC], 5) && window_y <= y && x >= window_x) {
            win_in_scanline = 1;
            tilemap_address = CHECK_BIT(mmu->mem[LCDC], 6) ? 0x9C00 : 0x9800;
            pos_y = ppu->wly;
            pos_x = x - window_x;
        } else {
            tilemap_address = CHECK_BIT(mmu->mem[LCDC], 3) ? 0x9C00 : 0x9800;
            pos_y = scroll_y + y;
            pos_x = scroll_x + x;
        }

        // find tile_id x,y coordinates in the memory "2D array" holding the tile_ids.
        // True y pos is SCY + LY. Then divide by 8 because tiles are 8x8 and multiply by 32 because this is a 32x32 "2D array" in a 1D array.
        word_t tile_id_y = (pos_y / 8) * 32;
        // True x pos is SCX + x. Then divide by 8 because tiles are 8x8.
        byte_t tile_id_x = pos_x / 8;
        word_t tile_id_address = tilemap_address + tile_id_x + tile_id_y;

        byte_t tile_attributes = mmu->vram_extra[tile_id_address - VRAM];

        // find the vertical line of the tile we are on (% 8 because tiles are 8 pixels tall, * 2 because each line takes 2 bytes of memory)
        byte_t tiledata_line;
        if (CHECK_BIT(tile_attributes, 6)) // flip y
            tiledata_line = (((pos_y % 8) - 7) * -1) * 2;
        else
            tiledata_line = (pos_y % 8) * 2;

        // get tiledata memory address
        word_t tiledata_address;
        s_word_t tile_id;
        if (CHECK_BIT(mmu->mem[LCDC], 4)) {
            tile_id = mmu->mem[tile_id_address];
            tiledata_address = (tile_id * 16) + tiledata_line; // each tile takes 16 bytes in memory (8x8 pixels, 2 byte per pixels)
        } else {
            tile_id = (s_byte_t) mmu->mem[tile_id_address] + 128;
            tiledata_address = 0x0800 + (tile_id * 16) + tiledata_line; // each tile takes 16 bytes in memory (8x8 pixels, 2 byte per pixels)
        }

        // 2. DRAW PIXEL

        // pixel color data takes 2 bytes in memory
        byte_t pixel_data_1;
        byte_t pixel_data_2;
        if (CHECK_BIT(tile_attributes, 3)) { // if tile is in VRAM bank 1
            pixel_data_1 = mmu->vram_extra[tiledata_address];
            pixel_data_2 = mmu->vram_extra[tiledata_address + 1];
        } else { // or in VRAM bank 0
            pixel_data_1 = mmu->mem[tiledata_address + VRAM];
            pixel_data_2 = mmu->mem[tiledata_address + VRAM + 1];
        }

        // bit 7 of 1st byte == low bit of pixel 0, bit 7 of 2nd byte == high bit of pixel 0
        // bit 6 of 1st byte == low bit of pixel 1, bit 6 of 2nd byte == high bit of pixel 1
        // etc.
        byte_t relevant_bit;
        if (CHECK_BIT(tile_attributes, 5)) // flip x
            relevant_bit = pos_x % 8;        
        else
            relevant_bit = ((pos_x % 8) - 7) * -1;

        // construct color data
        byte_t color_id = (GET_BIT(pixel_data_2, relevant_bit) << 1) | GET_BIT(pixel_data_1, relevant_bit);
        // cache color_id to be used for objects rendering
        ppu->scanline_cache_color_data[x] = color_id;

        byte_t color_palette_id = tile_attributes & 0x07;
        byte_t color_palette_address = color_palette_id * 8 + color_id * 2;
        word_t color_palette_data = (mmu->cram_bg[color_palette_address + 1] << 8) | mmu->cram_bg[color_palette_address];

        byte_t r, g, b;
        get_color_cgb(color_palette_data, &r, &g, &b);

        // set pixel color using BG (for background and window) palette data
        SET_PIXEL_CGB(ppu, x, y, r, g, b);
    }

    // increment ppu->wly if the window is present in this scanline
    if (win_in_scanline)
        ppu->wly++;
}

/**
 * Draws sprites of line LY
 */
static void draw_objects_cgb(emulator_t *emu) {
    ppu_t *ppu = emu->ppu;
    mmu_t *mmu = emu->mmu;

    // objects disabled
    if (!CHECK_BIT(mmu->mem[LCDC], 1))
        return;

    // are objects 8x8 or 8x16?
    byte_t obj_height = 8;
    if (CHECK_BIT(mmu->mem[LCDC], 2))
        obj_height = 16;

    byte_t is_using_x_priority = CHECK_BIT(mmu->mem[OPRI], 0);
    if (is_using_x_priority) {
        // default at 255 because no object will have that much of x coordinate (and if so, will not be visible anyway)
        memset(ppu->obj_pixel_priority, 255, GB_SCREEN_WIDTH);
    }

    byte_t y = mmu->mem[LY];

    // iterate over all objects to render
    for (short i = ppu->oam_scan.size - 1; i >= 0; i--) {
        word_t obj_address = ppu->oam_scan.objs_addresses[i];
        // obj y + 16 position is in byte 0
        s_word_t pos_y = mmu->mem[obj_address] - 16;
        // obj x + 8 position is in byte 1 (signed word important here!)
        s_word_t pos_x = mmu->mem[obj_address + 1] - 8;
        // obj position in the tile memory array
        byte_t tile_index = mmu->mem[obj_address + 2];
        if (obj_height > 8)
            tile_index &= 0xFE;

        // attributes flags
        byte_t flags = mmu->mem[obj_address + 3];

        // palette id
        word_t color_palette_id = flags & 0x07;

        // find line of the object we are currently on
        byte_t obj_line = y - pos_y;
        if (CHECK_BIT(flags, 6)) // flip y
            obj_line = (obj_line - (obj_height - 1)) * -1;
        obj_line *= 2; // each line takes 2 bytes in memory

        // find object tile data in memory
        word_t objdata_address;
        // pixel color data takes 2 bytes in memory
        byte_t pixel_data_1;
        byte_t pixel_data_2;
        if (CHECK_BIT(flags, 3)) { // if tile is in VRAM bank 1
            objdata_address = tile_index * 16;
            // pixel color data takes 2 bytes in memory
            pixel_data_1 = mmu->vram_extra[objdata_address + obj_line];
            pixel_data_2 = mmu->vram_extra[objdata_address + obj_line + 1];
        } else {
            objdata_address = VRAM + tile_index * 16;
            // pixel color data takes 2 bytes in memory
            pixel_data_1 = mmu->mem[objdata_address + obj_line];
            pixel_data_2 = mmu->mem[objdata_address + obj_line + 1];
        }

        // 3. DRAW OBJECT

        // bit 7 of 1st byte == low bit of pixel 0, bit 7 of 2nd byte == high bit of pixel 0
        // bit 6 of 1st byte == low bit of pixel 1, bit 6 of 2nd byte == high bit of pixel 1
        // etc.
        for (byte_t x = 0; x <= 7; x++) {
            s_word_t pixel_x = pos_x + x;
            // don't draw pixel if it's outside the screen
            if (pixel_x < 0 || pixel_x > 159)
                continue;

            byte_t relevant_bit = x; // flip x
            if (!CHECK_BIT(flags, 5)) // don't flip x
                relevant_bit = (relevant_bit - 7) * -1;

            // construct color data
            byte_t color_id = (GET_BIT(pixel_data_2, relevant_bit) << 1) | GET_BIT(pixel_data_1, relevant_bit);
            if (color_id == DMG_WHITE) // DMG_WHITE is the transparent color for objects, don't draw it (this needs to be done BEFORE palette conversion).
                continue;

            // if object priority bit is in dmg mode, priority is based on lower x coordinate
            if (is_using_x_priority) {
                // if an object with lower x coordinate has already drawn this pixel, it has higher priority so we abort
                if (ppu->obj_pixel_priority[pixel_x] < pos_x)
                    continue;

                // store current object x coordinate in priority array
                ppu->obj_pixel_priority[pixel_x] = pos_x;
            }

            // if object is below background, object can only be drawn current if background/window color (before applying palette) is 0
            if (CHECK_BIT(flags, 7) && ppu->scanline_cache_color_data[pixel_x])
                continue;

            byte_t color_palette_address = color_palette_id * 8 + color_id * 2;
            word_t color_palette_data = (mmu->cram_obj[color_palette_address + 1] << 8) | mmu->cram_obj[color_palette_address];

            byte_t r, g, b;
            get_color_cgb(color_palette_data, &r, &g, &b);

            // set pixel color using BG (for background and window) palette data
            SET_PIXEL_CGB(ppu, pixel_x, y, r, g, b);
        }
    }
}

static inline void oam_scan(emulator_t *emu) {
    ppu_t *ppu = emu->ppu;
    mmu_t *mmu = emu->mmu;

    // clear previous oam_scan
    ppu->oam_scan.size = 0;

    byte_t y = mmu->mem[LY];
    byte_t obj_height = 8;
    if (CHECK_BIT(mmu->mem[LCDC], 2))
        obj_height = 16;

    // iterate over all possible object in OAM memory
    for (byte_t obj_id = 0; obj_id < 40 && ppu->oam_scan.size < 10; obj_id++) {
        // each object takes 4 bytes in the OAM
        word_t obj_address = OAM + (obj_id * 4);

        // obj y + 16 position is in byte 0
        s_word_t pos_y = mmu->mem[obj_address] - 16;

        // don't render this object if it doesn't intercept the current scanline
        if (y < pos_y || y >= pos_y + obj_height)
            continue;

        ppu->oam_scan.objs_addresses[ppu->oam_scan.size++] = obj_address;
    }
}

/**
 * This does not implement the pixel FIFO but draws each scanline instantly when starting PPU_HBLANK (mode 0).
 * TODO if STAT interrupts are a problem, implement these corner cases: http://gameboy.mongenel.com/dmg/istat98.txt
 */
void ppu_step(emulator_t *emu, int cycles) {
    ppu_t *ppu = emu->ppu;
    mmu_t *mmu = emu->mmu;

    if (!CHECK_BIT(mmu->mem[LCDC], 7)) { // is LCD disabled?
        // TODO not sure of the handling of LCD disabled
        // TODO LCD disabled in DMG mode should fill screen with a color brighter than DMG_WHITE

        if (!ppu->is_lcd_turning_on) {
            // blank screen
            for (int i = 0; i < GB_SCREEN_WIDTH; i++) {
                for (int j = 0; j < GB_SCREEN_HEIGHT; j++) {
                    if (emu->mode == DMG) {
                        SET_PIXEL_DMG(ppu, i, j, DMG_WHITE);
                    } else {
                        byte_t r, g, b;
                        get_color_cgb(0xFFFF, &r, &g, &b);
                        SET_PIXEL_CGB(ppu, i, j, r, g, b);
                    }
                }

            }

            ppu->new_frame_cb(ppu->pixels);
            ppu->is_lcd_turning_on = 1;
        }

        PPU_SET_MODE(PPU_MODE_HBLANK);
        RESET_BIT(mmu->mem[STAT], 2); // set LYC=LY to 0?
        ppu->cycles = 0;
        mmu->mem[LY] = 0;
        ppu->wly = 0;
        return;
    }

    // if lcd was just turned on, check for LYC=LY
    if (ppu->is_lcd_turning_on)
        ppu_ly_lyc_compare(emu);

    ppu->cycles += cycles;

    switch (mmu->mem[STAT] & 0x03) { // switch current mode
    case PPU_MODE_OAM:
        if (ppu->cycles >= 80) {
            ppu->cycles -= 80;
            oam_scan(emu);
            PPU_SET_MODE(PPU_MODE_DRAWING);
        }
        break;
    case PPU_MODE_DRAWING:
        if (ppu->cycles >= 172) {
            ppu->cycles -= 172;
            PPU_SET_MODE(PPU_MODE_HBLANK);
            if (CHECK_BIT(mmu->mem[STAT], 3))
                cpu_request_interrupt(emu, IRQ_STAT);

            if (emu->mode == CGB) {
                draw_bg_win_cgb(emu);
                draw_objects_cgb(emu);
            } else {
                draw_bg_win(emu);
                draw_objects(emu);
            }
        }
        break;
    case PPU_MODE_HBLANK:
        if (ppu->cycles >= 204) {
            ppu->cycles -= 204;
            mmu->mem[LY]++;
            ppu_ly_lyc_compare(emu);

            if (mmu->mem[LY] == GB_SCREEN_HEIGHT) {
                PPU_SET_MODE(PPU_MODE_VBLANK);
                if (CHECK_BIT(mmu->mem[STAT], 4))
                    cpu_request_interrupt(emu, IRQ_STAT);

                cpu_request_interrupt(emu, IRQ_VBLANK);
                
                // skip first screen rendering after the LCD was just turned on
                if (ppu->is_lcd_turning_on) {
                    ppu->is_lcd_turning_on = 0;
                    break;
                }

                if (ppu->new_frame_cb)
                    ppu->new_frame_cb(ppu->pixels);
            } else {
                PPU_SET_MODE(PPU_MODE_OAM);
                if (CHECK_BIT(mmu->mem[STAT], 5))
                    cpu_request_interrupt(emu, IRQ_STAT);
            }
        }
        break;
    case PPU_MODE_VBLANK:
        if (ppu->cycles >= 456) {
            ppu->cycles -= 456;
            mmu->mem[LY]++;
            if (mmu->mem[LY] == 153)
                ppu_ly_lyc_compare(emu);

            if (mmu->mem[LY] == 154) {
                mmu->mem[LY] = 0;
                ppu_ly_lyc_compare(emu);
                ppu->wly = 0;
                PPU_SET_MODE(PPU_MODE_OAM);
                if (CHECK_BIT(mmu->mem[STAT], 5))
                    cpu_request_interrupt(emu, IRQ_STAT);
            }
        }
        break;
    }
}

void ppu_init(emulator_t *emu, void (*new_frame_cb)(byte_t *pixels)) {
    emu->ppu = xcalloc(1, sizeof(ppu_t));
    emu->ppu->new_frame_cb = new_frame_cb;
    emu->ppu->pixels = xmalloc(GB_SCREEN_WIDTH * GB_SCREEN_HEIGHT * 3);
    emu->ppu->scanline_cache_color_data = xmalloc(GB_SCREEN_WIDTH);
    emu->ppu->obj_pixel_priority = xmalloc(GB_SCREEN_WIDTH);
}

void ppu_quit(emulator_t *emu) {
    free(emu->ppu->pixels);
    free(emu->ppu->scanline_cache_color_data);
    free(emu->ppu->obj_pixel_priority);
    free(emu->ppu);
}
