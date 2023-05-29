#include <string.h>
#include <stdlib.h>

#include "emulator.h"
#include "ppu.h"
#include "mmu.h"
#include "cpu.h"

// Useful resources
// https://pixelbits.16-b.it/GBEDG/ppu/
// https://github.com/gbdev/pandocs/blob/bae52a72501a4ae872cf06ee0b26b3a83b274fca/src/Rendering_Internals.md
// https://github.com/trekawek/coffee-gb/tree/master/src/main/java/eu/rekawek/coffeegb/gpu

#define PPU_SET_MODE(mmu_ptr, mode) (mmu_ptr)->mem[STAT] = ((mmu_ptr)->mem[STAT] & 0xFC) | (mode)

#define IS_PPU_WIN_ENABLED(mmu_ptr) (CHECK_BIT((mmu_ptr)->mem[LCDC], 5))
#define IS_PPU_OBJ_TALL(mmu_ptr) (CHECK_BIT((mmu_ptr)->mem[LCDC], 2))
#define IS_PPU_OBJ_ENABLED(mmu_ptr) (CHECK_BIT((mmu_ptr)->mem[LCDC], 1))
#define IS_PPU_BG_WIN_ENABLED(mmu_ptr) (CHECK_BIT((mmu_ptr)->mem[LCDC], 0))

#define IS_PPU_BG_NORMAL_ADDRESS(mmu_ptr) CHECK_BIT((mmu_ptr)->mem[LCDC], 4)

#define SET_PIXEL_DMG(ppu_ptr, x, y, color, palette) { *((ppu_ptr)->pixels + ((y) * GB_SCREEN_WIDTH * 3) + ((x) * 3)) = ppu_color_palettes[(palette)][(color)][0]; \
                            *((ppu_ptr)->pixels + ((y) * GB_SCREEN_WIDTH * 3) + ((x) * 3) + 1) = ppu_color_palettes[(palette)][(color)][1]; \
                            *((ppu_ptr)->pixels + ((y) * GB_SCREEN_WIDTH * 3) + ((x) * 3) + 2) = ppu_color_palettes[(palette)][(color)][2]; }

#define SET_PIXEL_CGB(ppu_ptr, x, y, r, g, b) { *((ppu_ptr)->pixels + ((y) * GB_SCREEN_WIDTH * 3) + ((x) * 3)) = (r); \
                            *((ppu_ptr)->pixels + ((y) * GB_SCREEN_WIDTH * 3) + ((x) * 3) + 1) = (g); \
                            *((ppu_ptr)->pixels + ((y) * GB_SCREEN_WIDTH * 3) + ((x) * 3) + 2) = (b); }

byte_t ppu_color_palettes[PPU_COLOR_PALETTE_MAX][4][3] = {
    { // grayscale colors
        { 0xFF, 0xFF, 0xFF },
        { 0xAA, 0xAA, 0xAA },
        { 0x55, 0x55, 0x55 },
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

typedef struct {
    byte_t y;
    byte_t x;
    byte_t tile_id;
    byte_t attributes;
    // the order of the struct members is important!
} obj_t;

struct oam_scan_t {
    obj_t *objs[10]; // this is ordered on the x coord of the obj_t, popping an element is just increasing the index
    byte_t size;
    byte_t index; // used in oam mode to iterate over the oam memory, in drawing mode this is the first element of the objs array
    byte_t step;
} oam_scan;

typedef struct {
    byte_t color;
    word_t palette;
    byte_t obj_priority; // only on CGB mode
    byte_t bg_priority;
    byte_t is_obj;
} pixel_t;

#define FIFO_SIZE 8
typedef struct {
    pixel_t pixels[FIFO_SIZE];
    byte_t size;
    byte_t head;
    byte_t tail;
} pixel_fifo_t;

typedef enum {
    GET_TILE_ID,
    GET_TILE_ID_SLEEP,
    GET_TILE_SLICE_LOW,
    GET_TILE_SLICE_LOW_SLEEP,
    GET_TILE_SLICE_HIGH,
    GET_TILE_SLICE_HIGH_SLEEP,
    PUSH,
    PUSH_SLEEP
} pixel_fetcher_step_t;

typedef enum {
    FETCH_BG,
    FETCH_WIN,
    FETCH_OBJ
} pixel_fetcher_mode_t;

pixel_fetcher_mode_t old_mode; // TODO mode that the FETCH_OBJ has replaced

typedef struct {
    pixel_t pixels[8];
    pixel_fetcher_mode_t mode;
    pixel_fetcher_step_t step;
    byte_t curr_oam_index; // only relevant when step is FETCH_OBJ, holds the index of the obj to fetch in the oam_scan.objs array

    word_t tiledata_address;
    byte_t current_tile_id;
    byte_t x; // x position of the fetcher on the scanline
} pixel_fetcher_t;

// TODO get coffee.gb source, run it and debug using intellij to understand the fetcher and fifo

pixel_fifo_t bg_win_fifo;
pixel_fifo_t obj_fifo;
pixel_fetcher_t pixel_fetcher;

word_t scanline_duration = 0;

// FIXME window and sprite don't render correctly if they are positioned before the scanline (they should be cropped
//       the outside screen not shown and the inside screen shown)

// FIXME pokemon red: when pokemon slides to the left, a bit of the hand slides a little bit with it.

/**
 * @returns color after applying palette.
 */
static byte_t get_color(mmu_t *mmu, byte_t color_data, word_t palette_address) {
    // get relevant bits of the color palette the color_data maps to
    byte_t filter = 0; // initialize to 0 to shut gcc warnings
    switch (color_data) {
        case 0: filter = 0x03; break;
        case 1: filter = 0x0C; break;
        case 2: filter = 0x30; break;
        case 3: filter = 0xC0; break;
    }

    // return the color using the palette
    return (mmu->mem[palette_address] & filter) >> (color_data * 2);
}

static inline pixel_t *pixel_fifo_pop(pixel_fifo_t *fifo) {
    if (fifo->size == 0)
        return NULL;
    fifo->size--;
    byte_t old_head = fifo->head;
    fifo->head = (fifo->head + 1) % FIFO_SIZE;
    return &fifo->pixels[old_head];
}

static inline void pixel_fifo_push(pixel_fifo_t *fifo, pixel_t *pixel) {
    if (fifo->size == FIFO_SIZE)
        return;
    fifo->size++;
    fifo->pixels[fifo->tail] = *pixel;
    fifo->tail = (fifo->tail + 1) % FIFO_SIZE;
}

static inline void pixel_fifo_clear(pixel_fifo_t *fifo) {
    fifo->size = 0;
    fifo->head = 0;
    fifo->tail = 0;
}

byte_t lcd_x = 0;
byte_t discard_pixels = 0; // amount of pixels to discard at the start of a scanline due to SCX scrolling

/**
 * taken from https://stackoverflow.com/a/2602885
 */
static inline byte_t reverse_bits_order(byte_t b) {
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}

// TODO remove nested switch case
// TODO if the ppu vram access is blocked, the tile id, tile map, etc. reads are 0xFF
static inline void drawing_step(emulator_t *emu) {
    mmu_t *mmu = emu->mmu;

    // TODO drawing step takes longer time that it should?????
    // TODO can the fifo be full (size == 16) and the fetcher still attempts to push? If so, this is a bug

    if (IS_PPU_WIN_ENABLED(mmu) && mmu->mem[WY] <= mmu->mem[LY] && mmu->mem[WX] - 7 <= lcd_x && pixel_fetcher.mode == FETCH_BG) {
        // the fetcher must switch to window mode for the remaining of the scanline (except when an object needs to be drawn)
        pixel_fetcher.mode = FETCH_WIN;
        old_mode = FETCH_WIN;
        pixel_fetcher.step = GET_TILE_ID;
        pixel_fetcher.x = 0;
        emu->ppu->wly++;
        pixel_fifo_clear(&bg_win_fifo);
    }

    if (pixel_fetcher.mode != FETCH_OBJ && oam_scan.index < oam_scan.size && oam_scan.objs[oam_scan.index]->x <= lcd_x + 8) {
        if (IS_PPU_OBJ_ENABLED(mmu)) {
            // there is an obj at the new position
            pixel_fetcher.mode = FETCH_OBJ;
            pixel_fetcher.step = GET_TILE_ID;
            pixel_fetcher.curr_oam_index = oam_scan.index;
        } else {
            // TODO unsure of what to do in this case...
            //      should we allow a check for window??
        }
        oam_scan.index++;
    }

    // 1. FETCHING PIXELS

    byte_t tile_slice = 0;
    word_t tilemap_address = 0;
    switch (pixel_fetcher.step) {
    case GET_TILE_ID:
        switch (pixel_fetcher.mode) {
        case FETCH_BG:
            byte_t ly_scy = mmu->mem[LY] + mmu->mem[SCY]; // addition carried out in 8 bits (== result % 256)
            byte_t fetcher_x_scx = pixel_fetcher.x + mmu->mem[SCX]; // addition carried out in 8 bits (== result % 256)
            tilemap_address = 0x9800 | (GET_BIT(mmu->mem[LCDC], 3) << 10) | ((ly_scy / 8) << 5) | ((fetcher_x_scx / 8) & 0x1F);
            pixel_fetcher.current_tile_id = mmu->mem[tilemap_address];
            break;
        case FETCH_WIN:
            tilemap_address = 0x9800 | (GET_BIT(mmu->mem[LCDC], 6) << 10) | (((emu->ppu->wly / 8) & 0x1F) << 5) | ((pixel_fetcher.x / 8) & 0x1F);
            pixel_fetcher.current_tile_id = mmu->mem[tilemap_address];
            break;
        case FETCH_OBJ:
            pixel_fetcher.current_tile_id = oam_scan.objs[pixel_fetcher.curr_oam_index]->tile_id;
            break;
        }

        pixel_fetcher.step = GET_TILE_ID_SLEEP;
        break;
    case GET_TILE_ID_SLEEP:
        pixel_fetcher.step = GET_TILE_SLICE_LOW;
        break;
    case GET_TILE_SLICE_LOW:
        byte_t bit_12 = 0;
        switch (pixel_fetcher.mode) {
        case FETCH_BG:
            bit_12 = !((mmu->mem[LCDC] & 0x10) || (pixel_fetcher.current_tile_id & 0x80));
            byte_t ly_scy = mmu->mem[LY] + mmu->mem[SCY]; // addition carried out in 8 bits (== result % 256)
            pixel_fetcher.tiledata_address = 0x8000 | (bit_12 << 12) | (pixel_fetcher.current_tile_id << 4) | ((ly_scy % 8) << 1);
            // if bg/win is enabled, get color data, else get 0
            tile_slice = IS_PPU_BG_WIN_ENABLED(mmu) ? mmu->mem[pixel_fetcher.tiledata_address] : 0;
            break;
        case FETCH_WIN:
            bit_12 = !((mmu->mem[LCDC] & 0x10) || (pixel_fetcher.current_tile_id & 0x80));
            pixel_fetcher.tiledata_address = 0x8000 | (bit_12 << 12) | (pixel_fetcher.current_tile_id << 4) | ((emu->ppu->wly % 8) << 1);
            // if bg/win is enabled, get color data, else get 0
            tile_slice = IS_PPU_BG_WIN_ENABLED(mmu) ? mmu->mem[pixel_fetcher.tiledata_address] : 0;
            break;
        case FETCH_OBJ:
            byte_t flip_y = CHECK_BIT(oam_scan.objs[pixel_fetcher.curr_oam_index]->attributes, 6);
            byte_t actual_tile_id = oam_scan.objs[pixel_fetcher.curr_oam_index]->tile_id;

            if (IS_PPU_OBJ_TALL(mmu)) {
                byte_t is_top_tile = abs(mmu->mem[LY] - oam_scan.objs[pixel_fetcher.curr_oam_index]->y) > 8;
                CHANGE_BIT(actual_tile_id, 0, flip_y ? is_top_tile : !is_top_tile);
            }

            byte_t bits_3_1 = (mmu->mem[LY] - oam_scan.objs[pixel_fetcher.curr_oam_index]->y) % 8;
            if (flip_y)
                bits_3_1 = ~bits_3_1;
            bits_3_1 &= 0x07;

            pixel_fetcher.tiledata_address = 0x8000 | (actual_tile_id << 4) | (bits_3_1 << 1);

            byte_t flip_x = CHECK_BIT(oam_scan.objs[pixel_fetcher.curr_oam_index]->attributes, 5);
            tile_slice = flip_x ? reverse_bits_order(mmu->mem[pixel_fetcher.tiledata_address]) : mmu->mem[pixel_fetcher.tiledata_address];
            tile_slice = flip_x ? reverse_bits_order(mmu->mem[pixel_fetcher.tiledata_address]) : mmu->mem[pixel_fetcher.tiledata_address];
            break;
        }

        for (byte_t i = 0; i < 8; i++)
            pixel_fetcher.pixels[i].color = GET_BIT(tile_slice, 7 - i);

        pixel_fetcher.step = GET_TILE_SLICE_LOW_SLEEP;
        break;
    case GET_TILE_SLICE_LOW_SLEEP:
        pixel_fetcher.step = GET_TILE_SLICE_HIGH;
        break;
    case GET_TILE_SLICE_HIGH:
        switch (pixel_fetcher.mode) {
        case FETCH_BG:
        case FETCH_WIN:
            // if bg/win is enabled, get color data, else get 0
            tile_slice = IS_PPU_BG_WIN_ENABLED(mmu) ? mmu->mem[pixel_fetcher.tiledata_address + 1] : 0;
            for (byte_t i = 0; i < 8; i++) {
                pixel_fetcher.pixels[i].color |= GET_BIT(tile_slice, 7 - i) << 1;
                pixel_fetcher.pixels[i].palette = BGP;
                pixel_fetcher.pixels[i].is_obj = 0;
            }
            break;
        case FETCH_OBJ:
            byte_t flip_x = CHECK_BIT(oam_scan.objs[pixel_fetcher.curr_oam_index]->attributes, 5);
            tile_slice = flip_x ? reverse_bits_order(mmu->mem[pixel_fetcher.tiledata_address + 1]) : mmu->mem[pixel_fetcher.tiledata_address + 1];

            for (byte_t i = 0; i < 8; i++) {
                pixel_fetcher.pixels[i].color |= GET_BIT(tile_slice, 7 - i) << 1;
                pixel_fetcher.pixels[i].palette = CHECK_BIT(oam_scan.objs[pixel_fetcher.curr_oam_index]->attributes, 4) ? OBP1 : OBP0;
                pixel_fetcher.pixels[i].is_obj = 1;
                pixel_fetcher.pixels[i].bg_priority = CHECK_BIT(oam_scan.objs[pixel_fetcher.curr_oam_index]->attributes, 7);
            }
            break;
        }

        pixel_fetcher.step = GET_TILE_SLICE_HIGH_SLEEP;
        break;
    case GET_TILE_SLICE_HIGH_SLEEP:
        pixel_fetcher.step = PUSH;
        break;
    case PUSH:
        switch (pixel_fetcher.mode) {
        case FETCH_BG:
        case FETCH_WIN:
            if (bg_win_fifo.size == 0) {
                for (byte_t i = 0; i < 8; i++)
                    pixel_fifo_push(&bg_win_fifo, &pixel_fetcher.pixels[i]);
                pixel_fetcher.x += 8;
            } else {
                goto shift_pixels; // TODO this is ugly...
            }
            break;
        case FETCH_OBJ:
            // overwrite old transparent obj pixels, then fill rest of fifo with the last pixels of this obj to not overwrite any old pixels
            byte_t old_size = obj_fifo.size;
            for (byte_t i = 0; i < FIFO_SIZE; i++) {
                byte_t fifo_index = (i + obj_fifo.head) % FIFO_SIZE;

                if (i < old_size) {
                    if (obj_fifo.pixels[fifo_index].color == DMG_WHITE)
                        obj_fifo.pixels[fifo_index] = pixel_fetcher.pixels[i];
                } else {
                    pixel_fifo_push(&obj_fifo, &pixel_fetcher.pixels[i]);
                }
            }
            pixel_fetcher.mode = old_mode;
            break;
        }
        pixel_fetcher.step = PUSH_SLEEP;
        break;
    case PUSH_SLEEP:
        pixel_fetcher.step = GET_TILE_ID;
        break;
    }

    shift_pixels:

    // 2. SHIFTING PIXELS OUT TO THE LCD

    if (pixel_fetcher.mode == FETCH_OBJ)
        return;

    pixel_t *bg_win_pixel = pixel_fifo_pop(&bg_win_fifo);
    if (!bg_win_pixel)
        return;

    if (discard_pixels) {
        discard_pixels--;
        return;
    }

    byte_t color = bg_win_pixel->color;
    word_t palette = bg_win_pixel->palette;

    // merge pixels
    pixel_t *obj_pixel = pixel_fifo_pop(&obj_fifo);
    if (obj_pixel) {
        byte_t bg_over_obj = obj_pixel->bg_priority && bg_win_pixel->color != DMG_WHITE;
        if (obj_pixel->color != DMG_WHITE && !bg_over_obj) { // ignore transparent obj pixel and don't draw over another sprite (thus, respecting x priority)
            color = obj_pixel->color;
            palette = obj_pixel->palette;
        }
    }

    SET_PIXEL_DMG(emu->ppu, lcd_x, mmu->mem[LY], get_color(mmu, color, palette), emu->ppu_color_palette);
    lcd_x++;

    if (lcd_x >= GB_SCREEN_WIDTH) {
        // the new position is outside the screen, this scanline is done: go into HBLANK mode
        lcd_x = 0;
        pixel_fetcher.x = 0;

        // TODO
        // printf("%d\n", scanline_duration - 80);

        pixel_fetcher = (pixel_fetcher_t) { 0 };
        pixel_fifo_clear(&bg_win_fifo);
        pixel_fifo_clear(&obj_fifo);

        oam_scan.index = 0;

        PPU_SET_MODE(mmu, PPU_MODE_HBLANK);
        if (IS_HBLANK_IRQ_STAT_ENABLED(emu))
            CPU_REQUEST_INTERRUPT(emu, IRQ_STAT);
    }
}

static inline void oam_scan_step(mmu_t *mmu) {
    // it takes 2 cycles to scan one OBJ so we skip every other step
    if (!oam_scan.step) {
        oam_scan.step = 1;
        return;
    }

    obj_t *obj = (obj_t *) &mmu->mem[OAM + (oam_scan.index * 4)];
    byte_t obj_height = IS_PPU_OBJ_TALL(mmu) ? 16 : 8;
    // NOTE: obj->x != 0 condition should not be checked even if ultimate gameboy talk says it should
    if (oam_scan.size < 10 && (obj->y <= mmu->mem[LY] + 16) && (obj->y + obj_height > mmu->mem[LY] + 16)) {
        s_byte_t i;
        // if equal x: insert after so that the drawing doesn't overwrite the existing sprite (equal x -> first scanned obj priority)
        for (i = oam_scan.size - 1; i >= 0 && oam_scan.objs[i]->x > obj->x; i--)
            oam_scan.objs[i + 1] = oam_scan.objs[i];
        oam_scan.objs[i + 1] = obj;
        oam_scan.size++;
    }

    oam_scan.step = 0;
    oam_scan.index++;

    // if OAM scan has ended, switch to DRAWING mode
    if (oam_scan.index == 40) {
        oam_scan.step = 0;
        oam_scan.index = 0;

        discard_pixels = mmu->mem[SCX] % 8;
        old_mode = FETCH_BG;
        PPU_SET_MODE(mmu, PPU_MODE_DRAWING);
    }
}

static inline void hblank_step(emulator_t *emu) {
    mmu_t *mmu = emu->mmu;

    if (scanline_duration == 456) {
        mmu->mem[LY]++;

        ppu_ly_lyc_compare(emu);

        if (mmu->mem[LY] == GB_SCREEN_HEIGHT) {
            scanline_duration = 0;
            emu->ppu->wly = -1;
            PPU_SET_MODE(mmu, PPU_MODE_VBLANK);
            if (IS_VBLANK_IRQ_STAT_ENABLED(emu))
                CPU_REQUEST_INTERRUPT(emu, IRQ_STAT);

            CPU_REQUEST_INTERRUPT(emu, IRQ_VBLANK);
            if (emu->on_new_frame)
                emu->on_new_frame(emu->ppu->pixels);
        } else {
            oam_scan.size = 0;
            scanline_duration = 0;
            PPU_SET_MODE(mmu, PPU_MODE_OAM);
            if (IS_OAM_IRQ_STAT_ENABLED(emu))
                CPU_REQUEST_INTERRUPT(emu, IRQ_STAT);
        }
    }
}

static inline void vblank_step(emulator_t *emu) {
    mmu_t *mmu = emu->mmu;

    if (scanline_duration == 456) {
        mmu->mem[LY]++; // increase at each new line
        scanline_duration = 0;

        if (mmu->mem[LY] == 153) {
            // line 153 can trigger the LY=LYC interrupt
            ppu_ly_lyc_compare(emu);
        } else if (mmu->mem[LY] == 154) {
            mmu->mem[LY] = 0;
            ppu_ly_lyc_compare(emu);

            oam_scan.size = 0;
            scanline_duration = 0;
            PPU_SET_MODE(mmu, PPU_MODE_OAM);
            if (IS_OAM_IRQ_STAT_ENABLED(emu))
                CPU_REQUEST_INTERRUPT(emu, IRQ_STAT);
        }
    }
}

void ppu_step(emulator_t *emu) {
    ppu_t *ppu = emu->ppu;
    mmu_t *mmu = emu->mmu;

    if (!IS_LCD_ENABLED(emu)) {
        // TODO not sure of the handling of LCD disabled
        // TODO LCD disabled should fill screen with a color brighter than WHITE

        if (!ppu->is_lcd_turning_on) {
            // blank screen
            for (int i = 0; i < GB_SCREEN_WIDTH; i++)
                for (int j = 0; j < GB_SCREEN_HEIGHT; j++)
                    SET_PIXEL_DMG(ppu, i, j, DMG_WHITE, emu->ppu_color_palette);

            if (emu->on_new_frame)
                emu->on_new_frame(ppu->pixels);
            ppu->is_lcd_turning_on = 1;
        }
        PPU_SET_MODE(mmu, PPU_MODE_HBLANK);
        scanline_duration = 369; // (== 456 - 87) set hblank duration to its minimum possible value
        mmu->mem[LY] = 0;
        return;
    }

    // if lcd was just enabled, check for LYC=LY
    if (ppu->is_lcd_turning_on)
        ppu_ly_lyc_compare(emu);
    ppu->is_lcd_turning_on = 0;

    RESET_BIT(mmu->mem[STAT], 2); // reset LYC=LY flag

    int cycles = 4;
    while (cycles--) {
        switch (PPU_GET_MODE(emu)) {
        case PPU_MODE_OAM:
            // Scanning OAM for (X, Y) coordinates of sprites that overlap this line
            // there is up to 40 OBJs in OAM memory and this mode takes 80 cycles: 2 cycles per OBJ to check if it should be displayed
            // put them into an array of max size 10.
            oam_scan_step(mmu);
            break;
        case PPU_MODE_DRAWING:
            // Reading OAM and VRAM to generate the picture
            drawing_step(emu);
            break;
        case PPU_MODE_HBLANK:
            // Horizontal blanking
            hblank_step(emu);
            break;
        case PPU_MODE_VBLANK:
            // Vertical blanking
            vblank_step(emu);
            break;
        }

        scanline_duration++;
    }
}

void ppu_init(emulator_t *emu) {
    emu->ppu = xcalloc(1, sizeof(ppu_t));
    emu->ppu->wly = -1;
    emu->ppu->pixels = xmalloc(GB_SCREEN_WIDTH * GB_SCREEN_HEIGHT * 4);
    emu->ppu->scanline_cache_color_data = xmalloc(GB_SCREEN_WIDTH);
    emu->ppu->obj_pixel_priority = xmalloc(GB_SCREEN_WIDTH);
}

void ppu_quit(emulator_t *emu) {
    free(emu->ppu->pixels);
    free(emu->ppu->scanline_cache_color_data);
    free(emu->ppu->obj_pixel_priority);
    free(emu->ppu);
}

size_t ppu_serialized_length(UNUSED emulator_t *emu) {
    return 25;
}

byte_t *ppu_serialize(emulator_t *emu, size_t *size) {
    *size = ppu_serialized_length(emu);
    byte_t *buf = xmalloc(*size);
    memcpy(buf, &emu->ppu->is_lcd_turning_on, 1);
    memcpy(&buf[1], &emu->ppu->wly, 2);
    memcpy(&buf[3], &emu->ppu->cycles, 2);
    memcpy(&buf[5], &emu->ppu->oam_scan.objs_addresses, sizeof(emu->ppu->oam_scan.objs_addresses));
    memcpy(&buf[5 + sizeof(emu->ppu->oam_scan.objs_addresses)], &emu->ppu->oam_scan.size, 1);
    return buf;
}

void ppu_unserialize(emulator_t *emu, byte_t *buf) {
    memcpy(&emu->ppu->is_lcd_turning_on, buf, 1);
    memcpy(&emu->ppu->wly, &buf[1], 2);
    memcpy(&emu->ppu->cycles, &buf[3], 2);
    memcpy(&emu->ppu->oam_scan.objs_addresses, &buf[5], sizeof(emu->ppu->oam_scan.objs_addresses));
    memcpy(&emu->ppu->oam_scan.size, &buf[5 + sizeof(emu->ppu->oam_scan.objs_addresses)], 1);
}
