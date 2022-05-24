#include <string.h>

#include "ppu.h"
#include "utils.h"
#include "types.h"
#include "mmu.h"
#include "cpu.h"

#define PPU_SET_MODE(m) mmu.mem[STAT] = (mmu.mem[STAT] & 0xFC) | (m)

#define SET_PIXEL(buf, x, y, color) *(buf + ((y) * GB_SCREEN_WIDTH * 3) + ((x) * 3)) = ppu_color_palettes[ppu.current_color_palette][(color)][0]; \
                            *(buf + ((y) * GB_SCREEN_WIDTH * 3) + ((x) * 3) + 1) = ppu_color_palettes[ppu.current_color_palette][(color)][1]; \
                            *(buf + ((y) * GB_SCREEN_WIDTH * 3) + ((x) * 3) + 2) = ppu_color_palettes[ppu.current_color_palette][(color)][2];

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
    color_t color;
    byte_t palette;
    byte_t priority;
} pixel_t;

typedef struct {
    pixel_t pixels[8];
    byte_t size;
    byte_t head;
} pixel_fifo_t;

typedef enum {
    GET_TILE_ID,
    GET_TILE_SLICE_LOW = 3,
    GET_TILE_SLICE_HIGH = 5,
    PUSH = 7
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

ppu_t ppu;

static void update_blank_screen_color(void) {
    for (int i = 0; i < GB_SCREEN_WIDTH; i++) {
        for (int j = 0; j < GB_SCREEN_HEIGHT; j++) {
            SET_PIXEL(ppu.blank_pixels, i, j, WHITE);
        }
    }
}

/**
 * @returns color after applying palette.
 */
static color_t get_color(byte_t color_data, word_t palette_address) {
    // get relevant bits of the color palette the color_data maps to
    byte_t filter = 0; // initialize to 0 to shut gcc warnings
    switch (color_data) {
        case 0: filter = 0x03; break;
        case 1: filter = 0x0C; break;
        case 2: filter = 0x30; break;
        case 3: filter = 0xC0; break;
    }

    // return the color using the palette
    return (mmu.mem[palette_address] & filter) >> (color_data * 2);
}

static pixel_t *pixel_fifo_pop(pixel_fifo_t *fifo) {
    if (fifo->size == 0)
        return NULL;
    fifo->size--;
    return &fifo->pixels[fifo->head++];
}

static void pixel_fifo_push(pixel_fifo_t *fifo, pixel_t *pixels, byte_t flip_x) {
    if (fifo->size > 0)
        return;

    if (flip_x) {
        while (fifo->size < 8) {
            fifo->pixels[fifo->size] = pixels[7 - fifo->size];
            fifo->size++;
        }
    } else {
        while (fifo->size < 8) {
            fifo->pixels[fifo->size] = pixels[fifo->size];
            fifo->size++;
        }
    }
    fifo->head = 0;
}

byte_t lx = 0;
byte_t dropped_pixels = 0;
static inline void drawing_step(void) {
    // pixel fetcher step
    switch (pixel_fetcher.step) { // TODO make steps an enum
    case GET_TILE_ID:
        // TODO check if in bg or win mode (for now only bg mode is implemented)
        // pixel_fetcher.tilemap_address = 0x9800 | (GET_BIT(mmu.mem[LCDC], 6) << 10) | ((mmu.mem[WY] / 8) << 5) | (lx / 8);
        pixel_fetcher.tilemap_address = 0x9800 | (GET_BIT(mmu.mem[LCDC], 3) << 10) | (((mmu.mem[LY] + mmu.mem[SCY]) / 8) << 5) | ((lx + mmu.mem[SCX]) / 8);

        // TODO if the ppu vram access is blocked, the tile id read is 0xFF
        // TODO 9th bit computed as (LCDC & $10) && !(ID & $80)???
        pixel_fetcher.current_tile_id = mmu.mem[pixel_fetcher.tilemap_address];
        break;
    case GET_TILE_SLICE_LOW:
        // get tiledata memory address
        if (CHECK_BIT(mmu.mem[LCDC], 4)) {
            pixel_fetcher.tiledata_address = 0x8000 + pixel_fetcher.current_tile_id * 16; // each tile takes 16 bytes in memory (8x8 pixels, 2 byte per pixels)
        } else {
            s_word_t tile_id = (s_byte_t) pixel_fetcher.current_tile_id + 128;
            pixel_fetcher.tiledata_address = 0x8800 + tile_id * 16; // each tile takes 16 bytes in memory (8x8 pixels, 2 byte per pixels)
        }
        // add the vertical line of the tile we are on (% 8 because tiles are 8 pixels tall, * 2 because each line takes 2 bytes of memory)
        pixel_fetcher.tiledata_address += ((mmu.mem[SCY] + mmu.mem[LY]) % 8) * 2;

        byte_t tile_slice_lo = mmu.mem[pixel_fetcher.tiledata_address];
        for (byte_t i = 0; i < 8; i++)
            pixel_fetcher.pixels[i].color = GET_BIT(tile_slice_lo, 7 - i);
        break;
    case GET_TILE_SLICE_HIGH:
        byte_t tile_slice_hi = mmu.mem[pixel_fetcher.tiledata_address + 1];
        for (byte_t i = 0; i < 8; i++)
            pixel_fetcher.pixels[i].color |= GET_BIT(tile_slice_hi, 7 - i) << 1;
        break;
    case PUSH:
        pixel_fifo_push(&bg_fifo, pixel_fetcher.pixels, 0);
        break;
    }
    pixel_fetcher.step = (pixel_fetcher.step + 1) % 8;
    lx++;

    // OBJ fetcher is just check for both conditions, then replace bg_push by push_obj on the first obj that is in the lx + 8 range ?
    // as oam objs are sorted, the first one that matches is either the head of the list or none

    // bg fifo step
    pixel_t *bg_pixel = pixel_fifo_pop(&bg_fifo);

    // obj fifo step

    // merge and draw pixel
    if (bg_pixel) {
        SET_PIXEL(ppu.pixels, lx - 8, mmu.mem[LY], get_color(bg_pixel->color, BGP));
    }

    // TODO >= or > ?
    if (lx >= GB_SCREEN_WIDTH + 8) {
        lx = 0;
        dropped_pixels = 0;

        pixel_fetcher = (pixel_fetcher_t) { 0 };
        bg_fifo = (pixel_fifo_t) { 0 };
        obj_fifo = (pixel_fifo_t) { 0 };

        oam_scan.index = 0;

        PPU_SET_MODE(PPU_MODE_HBLANK);
        if (CHECK_BIT(mmu.mem[STAT], 3))
            cpu_request_interrupt(IRQ_STAT);
    }
}

static inline void oam_scan_step(void) {
    // it takes 2 cycles to scan one OBJ so we skip every other step
    if (!oam_scan.step) {
        oam_scan.step = 1;
        return;
    }

    obj_t *obj = (obj_t *) &mmu.mem[OAM + (oam_scan.index * 4)];
    byte_t obj_height = CHECK_BIT(mmu.mem[LCDC], 2) ? 16 : 8;
    // NOTE: obj->x != 0 condition should not be checked even if ultimate gameboy talk says it should
    if (oam_scan.size < 10 && (mmu.mem[LY] + 16 >= obj->y) && (mmu.mem[LY] + 16 < obj->y + obj_height)) {
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
// TODO hblank duration should be adaptive following what the time it took for DRAWING mode
static inline void hblank_step(void) {
    if (!hblank_duration) {
        mmu.mem[LY]++;
        if (mmu.mem[LY] == GB_SCREEN_HEIGHT) {
            PPU_SET_MODE(PPU_MODE_VBLANK);
            vblank_duration = 4560;
            vblank_line_duration = 456;
            if (CHECK_BIT(mmu.mem[STAT], 4))
                cpu_request_interrupt(IRQ_STAT);

            cpu_request_interrupt(IRQ_VBLANK);
            if (ppu.new_frame_cb)
                ppu.new_frame_cb(ppu.pixels);
        } else {
            PPU_SET_MODE(PPU_MODE_OAM);
            if (CHECK_BIT(mmu.mem[STAT], 5))
                cpu_request_interrupt(IRQ_STAT);
        }
    }
    hblank_duration--;
}

static inline void vblank_step(void) {
    if (!vblank_line_duration) {
        mmu.mem[LY]++; // increase at each new line
        vblank_line_duration = 456;
    }

    if (!vblank_duration) {
        if (mmu.mem[LY] == 154) {
            mmu.mem[LY] = 0;
            PPU_SET_MODE(PPU_MODE_OAM);
            if (CHECK_BIT(mmu.mem[STAT], 5))
                cpu_request_interrupt(IRQ_STAT);
        }
    }

    vblank_line_duration--;
    vblank_duration--;
}

byte_t sent_blank_pixels = 0;
void ppu_step(int cycles) {
    if (!CHECK_BIT(mmu.mem[LCDC], 7)) { // is LCD disabled?
        // TODO not sure of the handling of LCD disabled
        // TODO LCD disabled should fill screen with a color brighter than WHITE

        if (!sent_blank_pixels) {
            update_blank_screen_color();
            ppu.new_frame_cb(ppu.blank_pixels);
            sent_blank_pixels = 1;
        }
        PPU_SET_MODE(PPU_MODE_HBLANK);
        hblank_duration = 87; // set hblank duration to its minimum possible value
        mmu.mem[LY] = 0;
        return;
    }
    sent_blank_pixels = 0;

    RESET_BIT(mmu.mem[STAT], 2); // reset LYC=LY flag

    while (cycles--) {
        switch (mmu.mem[STAT] & 0x03) {
        case PPU_MODE_OAM:
            // Scanning OAM for (X, Y) coordinates of sprites that overlap this line
            // there is up to 40 OBJs in OAM memory and this mode takes 80 cycles: 2 cycles per OBJ to check if it should be displayed
            // put them into an array of max size 10.
            oam_scan_step();
            break;
        case PPU_MODE_DRAWING:
            // Reading OAM and VRAM to generate the picture
            drawing_step();
            break;
        case PPU_MODE_HBLANK:
            // Horizontal blanking
            hblank_step();
            break;
        case PPU_MODE_VBLANK:
            // Vertical blanking
            vblank_step();
            break;
        }
    }

    if (mmu.mem[LY] == mmu.mem[LYC]) { // LY == LYC compare // FIXME BEFORE FIXING EVERYTHING ELSE (this may be the source of a LOT of problems)
        SET_BIT(mmu.mem[STAT], 2);
        if (CHECK_BIT(mmu.mem[STAT], 6))
            cpu_request_interrupt(IRQ_STAT);
    } else {
        RESET_BIT(mmu.mem[STAT], 2);
    }
}

void ppu_init(void (*new_frame_cb)(byte_t *pixels)) {
    ppu = (ppu_t) { .new_frame_cb = new_frame_cb };
}
