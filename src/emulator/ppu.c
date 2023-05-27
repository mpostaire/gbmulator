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

#define PPU_SET_MODE(m) mmu->mem[STAT] = (mmu->mem[STAT] & 0xFC) | (m)

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
    byte_t tile_index;
    byte_t attributes;
    // the order of the struct members is important!
} obj_t;

struct oam_scan_t {
    obj_t objs[10];
    byte_t size;
    byte_t index; // used in oam mode to iterate over the oam memory, in drawing mode to iterate over the scanned objs array
    byte_t step;
} oam_scan;

typedef struct {
    byte_t color;
    byte_t palette;
    byte_t obj_priority; // only on CGB mode
    byte_t bg_priority;
} pixel_t;

typedef struct {
    pixel_t pixels[8];
    byte_t size;
    byte_t head;
} pixel_fifo_t;

typedef enum {
    GET_TILE_ID,
    GET_TILE_SLICE_LOW = 2,
    GET_TILE_SLICE_HIGH = 4,
    PUSH = 6
} pixel_fetcher_step_t;

typedef struct {
    pixel_t pixels[8];
    pixel_fetcher_step_t step;

    word_t tilemap_address;
    word_t tiledata_address;
    byte_t current_tile_id;
} pixel_fetcher_t;

pixel_fifo_t bg_fifo;
pixel_fifo_t obj_fifo;
pixel_fetcher_t pixel_fetcher;

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

static pixel_t *pixel_fifo_pop(pixel_fifo_t *fifo) {
    if (fifo->size == 0)
        return NULL;
    fifo->size--;
    return &fifo->pixels[fifo->head++];
}

static inline byte_t pixel_fifo_push(pixel_fifo_t *fifo, pixel_t *pixels, byte_t flip_x) {
    byte_t pixel_idx = flip_x ? 7 - fifo->size : fifo->size;
    fifo->pixels[fifo->size] = pixels[pixel_idx];
    fifo->size++;

    if (fifo->size == 8) {
        fifo->head = 0;
        return 1;
    }

    return 0;
}

byte_t tile_lx = 0; // x position on the scanline of the pisel fetcher divided by 8: because the pixel etcher fetches 8 pixels (1 tile row) at a time
byte_t lcd_lx = 0; // x position on the scanline available for drawing a pixel
byte_t dropped_pixels = 0;
static inline void drawing_step(emulator_t *emu) {
    mmu_t *mmu = emu->mmu;

    // pixel fetcher step
    byte_t tile_slice;
    // byte_t is_window = CHECK_BIT(mmu->mem[LCDC], 5) && mmu->mem[LY] >= mmu->mem[WY] && tile_lx == mmu->mem[WX] - 7;
    byte_t is_window = 0;
    // if (is_window)
    //     pixel_fetcher.step = GET_TILE_ID;

    switch (pixel_fetcher.step) { // TODO make steps an enum
    case GET_TILE_ID:
        // TODO check if in bg or win mode (for now only bg mode is implemented)
        if (is_window) {
            pixel_fetcher.tilemap_address = 0x9800 | (GET_BIT(mmu->mem[LCDC], 6) << 10) | ((mmu->mem[WY] / 8) << 5) | tile_lx;
        } else {
            byte_t ly_scy = mmu->mem[LY] + mmu->mem[SCY]; // addition carried out in 8 bits (== result % 256)
            byte_t tile_lx_scx = tile_lx + mmu->mem[SCX]; // addition carried out in 8 bits (== result % 256)
            pixel_fetcher.tilemap_address = 0x9800 | (GET_BIT(mmu->mem[LCDC], 3) << 10) | ((ly_scy / 8) << 5) | tile_lx_scx;
        }

        // TODO if the ppu vram access is blocked, the tile id read is 0xFF
        // TODO 9th bit computed as (LCDC & $10) && !(ID & $80)???
        pixel_fetcher.current_tile_id = mmu->mem[pixel_fetcher.tilemap_address];

        pixel_fetcher.step++;
        break;
    case GET_TILE_ID + 1:
        pixel_fetcher.step++;
        break;
    case GET_TILE_SLICE_LOW:
        // get tiledata memory address
        if (CHECK_BIT(mmu->mem[LCDC], 4)) {
            pixel_fetcher.tiledata_address = 0x8000 + pixel_fetcher.current_tile_id * 16; // each tile takes 16 bytes in memory (8x8 pixels, 2 byte per pixels)
        } else {
            s_word_t tile_id = (s_byte_t) pixel_fetcher.current_tile_id + 128;
            pixel_fetcher.tiledata_address = 0x8800 + tile_id * 16; // each tile takes 16 bytes in memory (8x8 pixels, 2 byte per pixels)
        }
        // add the vertical line of the tile we are on (% 8 because tiles are 8 pixels tall, * 2 because each line takes 2 bytes of memory)
        pixel_fetcher.tiledata_address += ((mmu->mem[SCY] + mmu->mem[LY]) % 8) * 2;

        // if bg/win is enabled, get color data, else get 0
        if (CHECK_BIT(mmu->mem[LCDC], 0))
            tile_slice = mmu->mem[pixel_fetcher.tiledata_address];
        else
            tile_slice = 0;

        for (byte_t i = 0; i < 8; i++)
            pixel_fetcher.pixels[i].color = GET_BIT(tile_slice, 7 - i);

        pixel_fetcher.step++;
        break;
    case GET_TILE_SLICE_LOW + 1:
        pixel_fetcher.step++;
        break;
    case GET_TILE_SLICE_HIGH:
        // if bg/win is enabled, get color data, else get 0
        if (CHECK_BIT(mmu->mem[LCDC], 0))
            tile_slice = mmu->mem[pixel_fetcher.tiledata_address + 1];
        else
            tile_slice = 0;

        for (byte_t i = 0; i < 8; i++)
            pixel_fetcher.pixels[i].color |= GET_BIT(tile_slice, 7 - i) << 1;

        pixel_fetcher.step++;
        break;
    case GET_TILE_SLICE_HIGH + 1:
        pixel_fetcher.step++;
        break;
    case PUSH: // PENDING_PUSH
        if (bg_fifo.size > 0)
            break;

        pixel_fetcher.step++;
        // fall through
    case PUSH + 1: // PUSH
        if (pixel_fifo_push(&bg_fifo, pixel_fetcher.pixels, 0)) {
            pixel_fetcher.step = GET_TILE_ID;
            tile_lx++;
        } else {
            return;
        }
        break;
    }

    // OBJ fetcher is just check for both conditions, then replace bg_push by push_obj on the first obj that is in the tile_lx + 8 range ?
    // as oam objs are sorted, the first one that matches is either the head of the list or none

    // bg fifo step
    pixel_t *bg_pixel = pixel_fifo_pop(&bg_fifo);
    // if there isn't any bg_pixel in the bg_fifo, don't draw anything
    if (!bg_pixel)
        return;

    // obj fifo step
    pixel_t *obj_pixel = pixel_fifo_pop(&obj_fifo);

    // merge and draw pixel
    SET_PIXEL_DMG(emu->ppu, lcd_lx, mmu->mem[LY], get_color(mmu, bg_pixel->color, BGP), emu->ppu_color_palette);
    lcd_lx++;

    // if (obj_pixel)
    //     SET_PIXEL_DMG(emu->ppu, tile_lx - 8, mmu->mem[LY], get_color(mmu, obj_pixel->color, obj_pixel->palette), emu->ppu_color_palette);

    if (tile_lx * 8 > GB_SCREEN_WIDTH) {
        tile_lx = 0;
        lcd_lx = 0;
        dropped_pixels = 0;

        pixel_fetcher = (pixel_fetcher_t) { 0 };
        bg_fifo = (pixel_fifo_t) { 0 };
        obj_fifo = (pixel_fifo_t) { 0 };

        oam_scan.index = 0;

        PPU_SET_MODE(PPU_MODE_HBLANK);
        if (CHECK_BIT(mmu->mem[STAT], 3))
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
    byte_t obj_height = CHECK_BIT(mmu->mem[LCDC], 2) ? 16 : 8;
    // NOTE: obj->x != 0 condition should not be checked even if ultimate gameboy talk says it should
    if (oam_scan.size < 10 && (mmu->mem[LY] + 16 >= obj->y) && (mmu->mem[LY] + 16 < obj->y + obj_height)) {
        oam_scan.objs[oam_scan.size++] = (obj_t) {
            .y = obj->y,
            .x = obj->x,
            .tile_index = obj->tile_index,
            .attributes = obj->attributes
        };
    }

    oam_scan.step = 0;
    oam_scan.index++;

    // if OAM scan has ended, switch to DRAWING mode
    if (oam_scan.index == 40) {
        oam_scan.step = 0;
        oam_scan.index = 0;

        PPU_SET_MODE(PPU_MODE_DRAWING);
    }
}

byte_t hblank_duration = 0;
word_t vblank_duration = 0;
word_t vblank_line_duration = 0;
// TODO hblank duration should be adaptive following what time it took for DRAWING mode
static inline void hblank_step(emulator_t *emu) {
    mmu_t *mmu = emu->mmu;

    if (!hblank_duration) {
        mmu->mem[LY]++;

        ppu_ly_lyc_compare(emu);

        if (mmu->mem[LY] == GB_SCREEN_HEIGHT) {
            PPU_SET_MODE(PPU_MODE_VBLANK);
            vblank_duration = 4560;
            vblank_line_duration = 456;
            if (CHECK_BIT(mmu->mem[STAT], 4))
                CPU_REQUEST_INTERRUPT(emu, IRQ_STAT);

            CPU_REQUEST_INTERRUPT(emu, IRQ_VBLANK);
            if (emu->on_new_frame)
                emu->on_new_frame(emu->ppu->pixels);
        } else {
            PPU_SET_MODE(PPU_MODE_OAM);
            if (CHECK_BIT(mmu->mem[STAT], 5))
                CPU_REQUEST_INTERRUPT(emu, IRQ_STAT);
        }
    }
    hblank_duration--;
}

static inline void vblank_step(emulator_t *emu) {
    mmu_t *mmu = emu->mmu;

    if (!vblank_line_duration) {
        mmu->mem[LY]++; // increase at each new line
        vblank_line_duration = 456;

        // line 153 can trigger the LY=LYC interrupt
        if (mmu->mem[LY] == 153)
            ppu_ly_lyc_compare(emu);
    }

    if (!vblank_duration) {
        // mmu->mem[LY] == 154 here
        mmu->mem[LY] = 0;
        ppu_ly_lyc_compare(emu);

        PPU_SET_MODE(PPU_MODE_OAM);
        if (CHECK_BIT(mmu->mem[STAT], 5))
            CPU_REQUEST_INTERRUPT(emu, IRQ_STAT);
    }

    vblank_line_duration--;
    vblank_duration--;
}

void ppu_step(emulator_t *emu) {
    ppu_t *ppu = emu->ppu;
    mmu_t *mmu = emu->mmu;

    if (!CHECK_BIT(mmu->mem[LCDC], 7)) { // is LCD disabled?
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
        PPU_SET_MODE(PPU_MODE_HBLANK);
        hblank_duration = 87; // set hblank duration to its minimum possible value
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
        switch (mmu->mem[STAT] & 0x03) {
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
    }
}

void ppu_init(emulator_t *emu) {
    emu->ppu = xcalloc(1, sizeof(ppu_t));
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
    memcpy(&buf[1], &emu->ppu->wly, 1);
    memcpy(&buf[2], &emu->ppu->cycles, 2);
    memcpy(&buf[4], &emu->ppu->oam_scan.objs_addresses, sizeof(emu->ppu->oam_scan.objs_addresses));
    memcpy(&buf[4 + sizeof(emu->ppu->oam_scan.objs_addresses)], &emu->ppu->oam_scan.size, 1);
    return buf;
}

void ppu_unserialize(emulator_t *emu, byte_t *buf) {
    memcpy(&emu->ppu->is_lcd_turning_on, buf, 1);
    memcpy(&emu->ppu->wly, &buf[1], 1);
    memcpy(&emu->ppu->cycles, &buf[2], 2);
    memcpy(&emu->ppu->oam_scan.objs_addresses, &buf[4], sizeof(emu->ppu->oam_scan.objs_addresses));
    memcpy(&emu->ppu->oam_scan.size, &buf[4 + sizeof(emu->ppu->oam_scan.objs_addresses)], 1);
}
