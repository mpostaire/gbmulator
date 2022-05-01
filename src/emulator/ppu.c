#include <string.h>

#include "ppu.h"
#include "utils.h"
#include "types.h"
#include "mmu.h"
#include "cpu.h"

#define PPU_SET_MODE(m) mem[STAT] = (mem[STAT] & 0xFC) | (m)

#define SET_PIXEL(buf, x, y, color) *(buf + ((y) * 160 * 3) + ((x) * 3)) = color_palettes[current_color_palette][(color)][0]; \
                            *(buf + ((y) * 160 * 3) + ((x) * 3) + 1) = color_palettes[current_color_palette][(color)][1]; \
                            *(buf + ((y) * 160 * 3) + ((x) * 3) + 2) = color_palettes[current_color_palette][(color)][2];

byte_t color_palettes[][4][3] = {
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
byte_t current_color_palette = 0;

byte_t pixels[160 * 144 * 3];
byte_t pixels_cache_color_data[160][144];

int ppu_cycles = 0;

void (*vblank_cb)(byte_t *pixels);

/**
 * @returns color after applying palette.
 */
static color get_color(byte_t color_data, word_t palette_address) {
    // get relevant bits of the color palette the color_data maps to
    byte_t filter = 0; // initialize to 0 to shut gcc warnings
    switch (color_data) {
        case 0: filter = 0x03; break;
        case 1: filter = 0x0C; break;
        case 2: filter = 0x30; break;
        case 3: filter = 0xC0; break;
    }

    // return the color using the palette
    return (mem[palette_address] & filter) >> (color_data * 2);
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
            SET_PIXEL(pixels, x, y, get_color(WHITE, BGP));
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
        s_word_t tile_id;
        if (CHECK_BIT(mem[LCDC], 4)) {
            tile_id = mem[tile_id_address];
            tiledata_address = 0x8000 + tile_id * 16; // each tile takes 16 bytes in memory (8x8 pixels, 2 byte per pixels)
        } else {
            tile_id = (s_byte_t) mem[tile_id_address] + 128;
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
        // cache color_data to be used for objects rendering
        pixels_cache_color_data[x][y] = color_data;
        // set pixel color using BG (for background and window) palette data
        SET_PIXEL(pixels, x, y, get_color(color_data, BGP));
    }
}

/**
 * Draws sprites of line LY
 */
static void draw_objects(void) {
    // 1. FIND UP TO 10 OBJECTS TO RENDER

    // objects disabled
    if (!CHECK_BIT(mem[LCDC], 1))
        return;

    byte_t y = mem[LY];
    byte_t rendered_objects = 0; // Keeps track of the number of rendered objects. There is a limit of 10 per scanline.
    word_t obj_to_render[10];

    // are objects 8x8 or 8x16?
    byte_t obj_height = 8;
    if (CHECK_BIT(mem[LCDC], 2))
        obj_height = 16;

    // iterate over all possible object in OAM memory
    for (byte_t obj_id = 0; obj_id < 40; obj_id++) {
        // each object takes 4 bytes in the OAM
        word_t obj_address = OAM + (obj_id * 4);

        // obj y + 16 position is in byte 0
        s_word_t pos_y = mem[obj_address] - 16;

        // don't render this object if it doesn't intercept the current scanline
        if (y < pos_y || y >= pos_y + obj_height)
            continue;

        obj_to_render[rendered_objects] = obj_address;
        if (++rendered_objects >= 10)
            break;
    }

    // return if no object on this line
    if (!rendered_objects)
        return;

    // 2. RENDER THE OBJECTS

    // store the x position of each pixel drawn by the highest priority object
    byte_t obj_pixel_priority[160];
    // default at 255 because no object will have that much of x coordinate (and if so, will not be visible anyway)
    memset(obj_pixel_priority, 255, sizeof(obj_pixel_priority));

    // iterate over all objects to render
    for (int obj = rendered_objects - 1; obj >= 0; obj--) {
        word_t obj_address = obj_to_render[obj];
        // obj y + 16 position is in byte 0
        s_word_t pos_y = mem[obj_address] - 16;
        // obj x + 8 position is in byte 1 (signed word important here!)
        s_word_t pos_x = mem[obj_address + 1] - 8;
        // obj position in the tile memory array
        byte_t tile_index = mem[obj_address + 2];
        if (obj_height > 8)
            tile_index &= 0xFE;
        // attributes flags
        byte_t flags = mem[obj_address + 3];

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
        word_t objdata_address = 0x8000 + tile_index * 16;

        // 3. DRAW OBJECT

        // pixel color data takes 2 bytes in memory
        byte_t pixel_data_1 = mem[objdata_address + obj_line];
        byte_t pixel_data_2 = mem[objdata_address + obj_line + 1];

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
            byte_t color_data = (GET_BIT(pixel_data_2, relevant_bit) << 1) | GET_BIT(pixel_data_1, relevant_bit);
            if (color_data == WHITE) // WHITE is the transparent color for objects, don't draw it (this needs to be done BEFORE palette conversion).
                continue;

            // if an object with lower x coordinate has already drawn this pixel, it has higher priority so we abort
            if (obj_pixel_priority[pixel_x] < pos_x)
                continue;

            // store current object x coordinate in priority array
            obj_pixel_priority[pixel_x] = pos_x;

            // if object is below background, object can only be drawn current if backgournd/window color (before applying palette) is 0
            if (CHECK_BIT(flags, 7) && pixels_cache_color_data[pixel_x][y])
                continue;

            // set pixel color using palette
            SET_PIXEL(pixels, pixel_x, y, get_color(color_data, palette));
        }
    }
}

static int lyc(void) {
    if (mem[LY] == mem[LYC]) { // LY == LYC compare // FIXME BEFORE FIXING EVERYTHING ELSE (this may be the source of a LOT of problems)
        SET_BIT(mem[STAT], 2);
        return CHECK_BIT(mem[STAT], 6);
    } else {
        RESET_BIT(mem[STAT], 2);
        return 0;
    }
}

/**
 * This does not implement the pixel FIFO but draws each scanline instantly when starting PPU_HBLANK (mode 0).
 * TODO if STAT interrupts are a problem, implement these corner cases: http://gameboy.mongenel.com/dmg/istat98.txt
 */
byte_t ignore = 0;
void ppu_step(int cycles) {
    if (!CHECK_BIT(mem[LCDC], 7)) { // is LCD disabled?
        // TODO not sure of the handling of LCD disabled
        // TODO LCD disabled should fill screen with a color brighter than WHITE
        PPU_SET_MODE(PPU_HBLANK);
        ppu_cycles = 0;
        mem[LY] = 0;
        return;
    }

    ppu_cycles += cycles;
    byte_t request_stat_irq = 0;
    RESET_BIT(mem[STAT], 2);

    switch (mem[STAT] & 0x03) { // switch current mode
    case PPU_OAM:
        if (ppu_cycles >= 80) {
            ppu_cycles -= 80;
            PPU_SET_MODE(PPU_DRAWING);
        }
        break;
    case PPU_DRAWING:
        if (ppu_cycles >= 172) {
            ppu_cycles -= 172;
            PPU_SET_MODE(PPU_HBLANK);
            request_stat_irq = CHECK_BIT(mem[STAT], 3);

            draw_bg_win();
            draw_objects();
        }
        break;
    case PPU_HBLANK:
        if (ppu_cycles >= 204) {
            ppu_cycles -= 204;
            mem[LY] += 1;
            ignore = 0;

            if (mem[LY] == 144) {
                PPU_SET_MODE(PPU_VBLANK);
                request_stat_irq = CHECK_BIT(mem[STAT], 4);

                vblank_cb(pixels);
                cpu_request_interrupt(IRQ_VBLANK);
            } else {
                PPU_SET_MODE(PPU_OAM);
                request_stat_irq = CHECK_BIT(mem[STAT], 5);
            }
        }
        break;
    case PPU_VBLANK:
        if (ppu_cycles >= 456) {
            ppu_cycles -= 456;
            mem[LY] += 1;
            ignore = 0;

            if (mem[LY] == 154) {
                mem[LY] = 0;
                PPU_SET_MODE(PPU_OAM);
                request_stat_irq = CHECK_BIT(mem[STAT], 5);
            }
        }
        break;
    }

    if (!ignore && lyc()) {
        ignore = 1;
        request_stat_irq = 1;
    }

    if (request_stat_irq)
        cpu_request_interrupt(IRQ_STAT);
}

void ppu_update_pixels_with_palette(byte_t new_palette) {
    // replace old color values of the pixels with the new ones according to the new palette
    for (int i = 0; i < 160; i++) {
        for (int j = 0; j < 144; j++) {
            byte_t *R = (pixels + ((j) * 160 * 3) + ((i) * 3)) ;
            byte_t *G = (pixels + ((j) * 160 * 3) + ((i) * 3) + 1);
            byte_t *B = (pixels + ((j) * 160 * 3) + ((i) * 3) + 2);

            // find which color is at pixel (i,j)
            color c;
            for (c = WHITE; c <= BLACK; c++) {
                if (*R == color_palettes[current_color_palette][c][0] &&
                    *G == color_palettes[current_color_palette][c][1] &&
                    *B == color_palettes[current_color_palette][c][2]) {

                    // replace old color value by the new one according to the new palette
                    *R = color_palettes[new_palette][c][0];
                    *G = color_palettes[new_palette][c][1];
                    *B = color_palettes[new_palette][c][2];
                    break;
                }
            }
        }
    }
}

byte_t *ppu_get_pixels(void) {
    return pixels;
}

byte_t *ppu_get_color_values(enum color color) {
    return color_palettes[current_color_palette][color];
}

void ppu_set_color_palette(enum ppu_color_palette palette) {
    if (palette == PPU_COLOR_PALETTE_GRAY || palette == PPU_COLOR_PALETTE_ORIG)
        current_color_palette = palette;
}

byte_t ppu_get_color_palette(void) {
    return current_color_palette;
}

void ppu_set_vblank_callback(void (*vblank_callback)(byte_t *pixels)) {
    vblank_cb = vblank_callback;
}