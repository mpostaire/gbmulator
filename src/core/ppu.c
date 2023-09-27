#include <string.h>
#include <stdlib.h>

#include "gb_priv.h"
#include "serialize.h"

// Useful resources
// https://pixelbits.16-b.it/GBEDG/ppu/
// https://github.com/gbdev/pandocs/blob/bae52a72501a4ae872cf06ee0b26b3a83b274fca/src/Rendering_Internals.md
// https://github.com/trekawek/coffee-gb/tree/master/src/main/java/eu/rekawek/coffeegb/gpu

// TODO read this: http://gameboy.mongenel.com/dmg/istat98.txt
// TODO read this for fetcher timings: https://www.reddit.com/r/EmuDev/comments/s6cpis/comment/htlwkx9

#define PPU_SET_MODE(mmu_ptr, mode) (mmu_ptr)->io_registers[STAT - IO] = ((mmu_ptr)->io_registers[STAT - IO] & 0xFC) | (mode)

#define IS_PPU_WIN_ENABLED(mmu_ptr) (CHECK_BIT((mmu_ptr)->io_registers[LCDC - IO], 5))
#define IS_PPU_OBJ_TALL(mmu_ptr) (CHECK_BIT((mmu_ptr)->io_registers[LCDC - IO], 2))
#define IS_PPU_OBJ_ENABLED(mmu_ptr) (CHECK_BIT((mmu_ptr)->io_registers[LCDC - IO], 1))
#define IS_PPU_BG_WIN_ENABLED(mmu_ptr) (CHECK_BIT((mmu_ptr)->io_registers[LCDC - IO], 0))

#define IS_PPU_BG_NORMAL_ADDRESS(mmu_ptr) CHECK_BIT((mmu_ptr)->io_registers[LCDC - IO], 4)

#define IS_CGB_COMPAT_MODE(mmu_ptr) ((((mmu_ptr)->io_registers[KEY0 - IO] >> 2) & 0x03) == 1)

#define SET_PIXEL_DMG(emu_ptr, x, y, color)                                                                                     \
    do {                                                                                                                        \
        (emu_ptr)->ppu->pixels[((y) * GB_SCREEN_WIDTH * 4) + ((x) * 4)] = dmg_palettes[(emu_ptr)->dmg_palette][(color)][0];     \
        (emu_ptr)->ppu->pixels[((y) * GB_SCREEN_WIDTH * 4) + ((x) * 4) + 1] = dmg_palettes[(emu_ptr)->dmg_palette][(color)][1]; \
        (emu_ptr)->ppu->pixels[((y) * GB_SCREEN_WIDTH * 4) + ((x) * 4) + 2] = dmg_palettes[(emu_ptr)->dmg_palette][(color)][2]; \
        (emu_ptr)->ppu->pixels[((y) * GB_SCREEN_WIDTH * 4) + ((x) * 4) + 3] = 0xFF;                                             \
    } while (0)

#define SET_PIXEL_CGB(emu_ptr, x, y, r, g, b)                                       \
    do {                                                                            \
        (emu_ptr)->ppu->pixels[((y) * GB_SCREEN_WIDTH * 4) + ((x) * 4)] = (r);      \
        (emu_ptr)->ppu->pixels[((y) * GB_SCREEN_WIDTH * 4) + ((x) * 4) + 1] = (g);  \
        (emu_ptr)->ppu->pixels[((y) * GB_SCREEN_WIDTH * 4) + ((x) * 4) + 2] = (b);  \
        (emu_ptr)->ppu->pixels[((y) * GB_SCREEN_WIDTH * 4) + ((x) * 4) + 3] = 0xFF; \
    } while (0)

byte_t dmg_palettes[PPU_COLOR_PALETTE_MAX][4][3] = {
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

/**
 * @returns dmg color after applying palette.
 */
static inline gb_dmg_color_t dmg_get_color(gb_mmu_t *mmu, gb_pixel_t *pixel, byte_t is_obj) {
    // return the color using the palette
    word_t palette_address = is_obj ? OBP0 + pixel->palette : BGP;
    return (mmu->io_registers[palette_address - IO] >> (pixel->color << 1)) & 0x03;
}

/**
 * @returns cgb colors in r, g, b arguments after applying palette.
 */
static inline void cgb_get_color(gb_mmu_t *mmu, gb_pixel_t *pixel, byte_t is_obj, byte_t enable_color_correction, byte_t *r, byte_t *g, byte_t *b) {
    word_t color_data;
    if (pixel) {
        byte_t color_address = pixel->palette * 8 + pixel->color * 2;
        if (is_obj)
            color_data = (mmu->cram_obj[color_address + 1] << 8) | mmu->cram_obj[color_address];
        else
            color_data = (mmu->cram_bg[color_address + 1] << 8) | mmu->cram_bg[color_address];
    } else {
        color_data = 0xFFFF;
    }

    *r = color_data & 0x1F;
    *g = (color_data & 0x3E0) >> 5;
    *b = (color_data & 0x7C00) >> 10;

    if (enable_color_correction) {
        int R = (26 * *r) + (4 * *g) + (2 * *b);
        int G = (24 * *g) + (8 * *b);
        int B = (6 * *r) + (4 * *g) + (22 * *b);

        *r = MIN(960, R) >> 2;
        *g = MIN(960, G) >> 2;
        *b = MIN(960, B) >> 2;
    } else {
        // from 5 bit depth to 8 bit depth (no color correction)
        *r = (*r << 3) | (*r >> 2);
        *g = (*g << 3) | (*g >> 2);
        *b = (*b << 3) | (*b >> 2);
    }
}

void ppu_ly_lyc_compare(gb_t *gb) {
    gb_mmu_t *mmu = gb->mmu;

    if (mmu->io_registers[LY - IO] == mmu->io_registers[LYC - IO]) {
        SET_BIT(mmu->io_registers[STAT - IO], 2);
        if (IS_LY_LYC_IRQ_STAT_ENABLED(gb))
            CPU_REQUEST_INTERRUPT(gb, IRQ_STAT);
    } else {
        RESET_BIT(mmu->io_registers[STAT - IO], 2);
    }
}

static inline byte_t cgb_get_bg_win_tile_attributes(gb_t *gb) {
    gb_mmu_t *mmu = gb->mmu;
    gb_ppu_t *ppu = gb->ppu;

    word_t tile_id_address;
    switch (ppu->pixel_fetcher.mode) {
    case FETCH_BG: {
        byte_t ly_scy = mmu->io_registers[LY - IO] + mmu->io_registers[SCY - IO]; // addition carried out in 8 bits (== result % 256)
        byte_t fetcher_x_scx = ppu->pixel_fetcher.x + mmu->io_registers[SCX - IO]; // addition carried out in 8 bits (== result % 256)
        tile_id_address = 0x9800 | (GET_BIT(mmu->io_registers[LCDC - IO], 3) << 10) | ((ly_scy / 8) << 5) | ((fetcher_x_scx / 8) & 0x1F);
        break;
    }
    case FETCH_WIN:
        tile_id_address = 0x9800 | (GET_BIT(mmu->io_registers[LCDC - IO], 6) << 10) | (((ppu->wly / 8) & 0x1F) << 5) | ((ppu->pixel_fetcher.x / 8) & 0x1F);
        break;
    default:
        eprintf("this function can't be used for objs");
        exit(EXIT_FAILURE);
        break;
    }
    return mmu->vram[tile_id_address - VRAM + VRAM_BANK_SIZE]; // read inside VRAM bank 1
}

// TODO if the ppu vram access is blocked, the tile id, tile map, etc. reads are 0xFF
static inline word_t get_bg_tiledata_address(gb_t *gb, byte_t is_high, byte_t flip_y) {
    gb_mmu_t *mmu = gb->mmu;
    gb_ppu_t *ppu = gb->ppu;

    byte_t bit_12 = !((mmu->io_registers[LCDC - IO] & 0x10) || (ppu->pixel_fetcher.current_tile_id & 0x80));
    byte_t ly_scy = mmu->io_registers[LY - IO] + mmu->io_registers[SCY - IO]; // addition carried out in 8 bits (== result % 256)
    if (flip_y)
        ly_scy = ~ly_scy;
    ly_scy &= 0x07; // modulo 8
    return 0x8000 | (bit_12 << 12) | (ppu->pixel_fetcher.current_tile_id << 4) | (ly_scy << 1) | (!!is_high);
}

static inline word_t get_win_tiledata_address(gb_t *gb, byte_t is_high, byte_t flip_y) {
    gb_mmu_t *mmu = gb->mmu;
    gb_ppu_t *ppu = gb->ppu;

    byte_t bit_12 = !((mmu->io_registers[LCDC - IO] & 0x10) || (ppu->pixel_fetcher.current_tile_id & 0x80));
    s_word_t wly = ppu->wly;
    if (flip_y)
        wly = ~wly;
    wly &= 0x07; // modulo 8
    return 0x8000 | (bit_12 << 12) | (ppu->pixel_fetcher.current_tile_id << 4) | (wly << 1) | (!!is_high);
}

static inline word_t get_obj_tiledata_address(gb_t *gb, byte_t is_high) {
    gb_mmu_t *mmu = gb->mmu;
    gb_ppu_t *ppu = gb->ppu;

    gb_obj_t obj = ppu->oam_scan.objs[ppu->pixel_fetcher.curr_oam_index];

    byte_t flip_y = CHECK_BIT(obj.attributes, 6);
    byte_t actual_tile_id = obj.tile_id;

    if (IS_PPU_OBJ_TALL(mmu)) {
        byte_t is_top_tile = abs(mmu->io_registers[LY - IO] - obj.y) > 8;
        CHANGE_BIT(actual_tile_id, 0, flip_y ? is_top_tile : !is_top_tile);
    }

    byte_t bits_3_1 = (mmu->io_registers[LY - IO] - obj.y) % 8;
    if (flip_y)
        bits_3_1 = ~bits_3_1;
    bits_3_1 &= 0x07; // modulo 8

    return 0x8000 | (actual_tile_id << 4) | (bits_3_1 << 1) | (!!is_high);
}

static inline gb_pixel_t *pixel_fifo_pop(gb_pixel_fifo_t *fifo) {
    if (fifo->size == 0)
        return NULL;
    fifo->size--;
    byte_t old_head = fifo->head;
    fifo->head = (fifo->head + 1) % PIXEL_FIFO_SIZE;
    return &fifo->pixels[old_head];
}

static inline void pixel_fifo_push(gb_pixel_fifo_t *fifo, gb_pixel_t *pixel) {
    if (fifo->size == PIXEL_FIFO_SIZE)
        return;
    fifo->size++;
    fifo->pixels[fifo->tail] = *pixel;
    fifo->tail = (fifo->tail + 1) % PIXEL_FIFO_SIZE;
}

static inline void pixel_fifo_clear(gb_pixel_fifo_t *fifo) {
    fifo->size = 0;
    fifo->head = 0;
    fifo->tail = 0;
}

static inline void reset_pixel_fetcher(gb_ppu_t *ppu) {
    ppu->pixel_fetcher.x = 0;
    ppu->pixel_fetcher.step = 0;
    ppu->pixel_fetcher.mode = FETCH_BG;
    ppu->pixel_fetcher.init_delay_done = 0;
    ppu->pixel_fetcher.old_mode = FETCH_BG;
}

/**
 * taken from https://stackoverflow.com/a/2602885
 */
static inline byte_t reverse_bits_order(byte_t b) {
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}

static inline void fetch_tile_id(gb_t *gb) {
    gb_mmu_t *mmu = gb->mmu;
    gb_ppu_t *ppu = gb->ppu;

    switch (ppu->pixel_fetcher.mode) {
        case FETCH_BG: {
            byte_t ly_scy = mmu->io_registers[LY - IO] + mmu->io_registers[SCY - IO]; // addition carried out in 8 bits (== result % 256)
            byte_t fetcher_x_scx = ppu->pixel_fetcher.x + mmu->io_registers[SCX - IO]; // addition carried out in 8 bits (== result % 256)
            word_t tile_id_address = 0x9800 | (GET_BIT(mmu->io_registers[LCDC - IO], 3) << 10) | ((ly_scy / 8) << 5) | ((fetcher_x_scx / 8) & 0x1F);
            ppu->pixel_fetcher.current_tile_id = mmu->vram[tile_id_address - VRAM];
            break;
        }
        case FETCH_WIN: {
            word_t tile_id_address = 0x9800 | (GET_BIT(mmu->io_registers[LCDC - IO], 6) << 10) | (((gb->ppu->wly / 8) & 0x1F) << 5) | ((ppu->pixel_fetcher.x / 8) & 0x1F);
            ppu->pixel_fetcher.current_tile_id = mmu->vram[tile_id_address - VRAM];
            break;
        }
        case FETCH_OBJ: {
            gb_obj_t obj = ppu->oam_scan.objs[ppu->pixel_fetcher.curr_oam_index];
            ppu->pixel_fetcher.current_tile_id = obj.tile_id;
            break;
        }
    }
}

static inline byte_t get_tileslice(gb_t *gb, word_t tiledata_address, byte_t attributes) {
    gb_mmu_t *mmu = gb->mmu;

    byte_t flip_x = CHECK_BIT(attributes, 5);
    if (gb->mode == GB_MODE_CGB) {
        if (CHECK_BIT(attributes, 3)) // tile is in VRAM bank 1
            return flip_x ? reverse_bits_order(mmu->vram[tiledata_address - VRAM + VRAM_BANK_SIZE]) : mmu->vram[tiledata_address - VRAM + VRAM_BANK_SIZE];
        else
            return flip_x ? reverse_bits_order(mmu->vram[tiledata_address - VRAM]) : mmu->vram[tiledata_address - VRAM];
    } else {
        return flip_x ? reverse_bits_order(mmu->vram[tiledata_address - VRAM]) : mmu->vram[tiledata_address - VRAM];
    }
}

static inline void fetch_tileslice_low(gb_t *gb) {
    gb_ppu_t *ppu = gb->ppu;

    byte_t tileslice = 0;
    switch (ppu->pixel_fetcher.mode) {
        case FETCH_BG: {
            byte_t attributes = gb->mode == GB_MODE_CGB ? cgb_get_bg_win_tile_attributes(gb) : 0;
            word_t tiledata_address = get_bg_tiledata_address(gb, 0, CHECK_BIT(attributes, 6));
            tileslice = get_tileslice(gb, tiledata_address, attributes);
            break;
        }
        case FETCH_WIN: {
            byte_t attributes = gb->mode == GB_MODE_CGB ? cgb_get_bg_win_tile_attributes(gb) : 0;
            word_t tiledata_address = get_win_tiledata_address(gb, 0, CHECK_BIT(attributes, 6));
            tileslice = get_tileslice(gb, tiledata_address, attributes);
            break;
        }
        case FETCH_OBJ: {
            byte_t attributes = ppu->oam_scan.objs[ppu->pixel_fetcher.curr_oam_index].attributes;
            word_t tiledata_address = get_obj_tiledata_address(gb, 0);
            tileslice = get_tileslice(gb, tiledata_address, attributes);
            break;
        }
    }

    for (byte_t i = 0; i < 8; i++)
        ppu->pixel_fetcher.pixels[i].color = GET_BIT(tileslice, 7 - i);
}

static inline void fetch_tileslice_high(gb_t *gb) {
    gb_mmu_t *mmu = gb->mmu;
    gb_ppu_t *ppu = gb->ppu;

    byte_t tileslice = 0;
    word_t palette = 0;
    byte_t attributes = 0;
    byte_t oam_pos = 0;
    // this recalculate the tiledata address because the ppu doesn't cache this info between steps,
    // leading to potential bitplane desync
    switch (ppu->pixel_fetcher.mode) {
        case FETCH_BG: {
            if (gb->mode == GB_MODE_CGB) {
                attributes = cgb_get_bg_win_tile_attributes(gb);
                palette = IS_CGB_COMPAT_MODE(mmu) ? 0 : attributes & 0x07;
            } else {
                palette = 0;
            }
            word_t tiledata_address = get_bg_tiledata_address(gb, 1, CHECK_BIT(attributes, 6));
            tileslice = get_tileslice(gb, tiledata_address, attributes);
            break;
        }
        case FETCH_WIN: {
            if (gb->mode == GB_MODE_CGB) {
                attributes = cgb_get_bg_win_tile_attributes(gb);
                palette = IS_CGB_COMPAT_MODE(mmu) ? 0 : attributes & 0x07;
            } else {
                palette = 0;
            }
            word_t tiledata_address = get_win_tiledata_address(gb, 1, CHECK_BIT(attributes, 6));
            tileslice = get_tileslice(gb, tiledata_address, attributes);
            break;
        }
        case FETCH_OBJ: {
            attributes = ppu->oam_scan.objs[ppu->pixel_fetcher.curr_oam_index].attributes;
            word_t tiledata_address = get_obj_tiledata_address(gb, 1);
            tileslice = get_tileslice(gb, tiledata_address, attributes);
            oam_pos = ppu->oam_scan.objs_oam_pos[ppu->pixel_fetcher.curr_oam_index];

            byte_t is_cgb_mode = gb->mode == GB_MODE_CGB && !IS_CGB_COMPAT_MODE(mmu);
            palette = is_cgb_mode ? attributes & 0x07 : GET_BIT(attributes, 4);
            break;
        }
    }

    for (byte_t i = 0; i < 8; i++) {
        ppu->pixel_fetcher.pixels[i].color |= GET_BIT(tileslice, 7 - i) << 1;        
        ppu->pixel_fetcher.pixels[i].palette = palette;
        ppu->pixel_fetcher.pixels[i].attributes = attributes;
        ppu->pixel_fetcher.pixels[i].oam_pos = oam_pos;
    }
}

static inline byte_t push(gb_t *gb) {
    gb_ppu_t *ppu = gb->ppu;

    switch (ppu->pixel_fetcher.mode) {
        case FETCH_BG:
        case FETCH_WIN:
            if (ppu->bg_win_fifo.size > 0)
                return 0;

            for (byte_t i = 0; i < 8; i++)
                pixel_fifo_push(&ppu->bg_win_fifo, &ppu->pixel_fetcher.pixels[i]);
            ppu->pixel_fetcher.x += 8;
            break;
        case FETCH_OBJ: {
            // if the obj starts before the beginning of the scanline, only push the visible pixels by using an offset
            byte_t old_size = ppu->obj_fifo.size;
            byte_t offset = 0;
            if (ppu->oam_scan.objs[ppu->pixel_fetcher.curr_oam_index].x < 8)
                offset = 8 - ppu->oam_scan.objs[ppu->pixel_fetcher.curr_oam_index].x;

            byte_t is_cgb_mode = gb->mode == GB_MODE_CGB && !CHECK_BIT(gb->mmu->io_registers[OPRI - IO], 0);
            for (byte_t fetcher_index = offset; fetcher_index < PIXEL_FIFO_SIZE; fetcher_index++) {
                byte_t fifo_index = ((fetcher_index - offset) + ppu->obj_fifo.head) % PIXEL_FIFO_SIZE;

                if (fetcher_index < old_size) {
                    if (ppu->pixel_fetcher.pixels[fetcher_index].color == DMG_WHITE)
                        continue;
                    if (is_cgb_mode && ppu->pixel_fetcher.pixels[fetcher_index].oam_pos < ppu->obj_fifo.pixels[fifo_index].oam_pos)
                        ppu->obj_fifo.pixels[fifo_index] = ppu->pixel_fetcher.pixels[fetcher_index];
                    if (ppu->obj_fifo.pixels[fifo_index].color == DMG_WHITE)
                        ppu->obj_fifo.pixels[fifo_index] = ppu->pixel_fetcher.pixels[fetcher_index];
                } else {
                    pixel_fifo_push(&ppu->obj_fifo, &ppu->pixel_fetcher.pixels[fetcher_index]);
                }
            }

            ppu->pixel_fetcher.mode = ppu->pixel_fetcher.old_mode;
            break;
        }
    }

    return 1;
}

static inline gb_pixel_t *select_pixel(gb_t *gb, gb_pixel_t *bg_win_pixel, gb_pixel_t *obj_pixel) {
    gb_mmu_t *mmu = gb->mmu;

    byte_t is_cgb_mode = gb->mode == GB_MODE_CGB && !IS_CGB_COMPAT_MODE(mmu);
    if (is_cgb_mode) {
        if (!obj_pixel || !IS_PPU_OBJ_ENABLED(mmu) || obj_pixel->color == DMG_WHITE)
            return bg_win_pixel;
        if (bg_win_pixel->color == DMG_WHITE || !IS_PPU_BG_WIN_ENABLED(mmu))
            return obj_pixel;
        if (!CHECK_BIT(bg_win_pixel->attributes, 7) && !CHECK_BIT(obj_pixel->attributes, 7))
            return obj_pixel;
        return bg_win_pixel;
    } else {
        if (!IS_PPU_BG_WIN_ENABLED(mmu)) {
            bg_win_pixel->color = DMG_WHITE;
            // TODO keep palette / change palette / force white?
        }

        if (!obj_pixel)
            return bg_win_pixel;
        if (!IS_PPU_OBJ_ENABLED(mmu))
            return bg_win_pixel;

        byte_t bg_over_obj = CHECK_BIT(obj_pixel->attributes, 7) && bg_win_pixel->color != DMG_WHITE;
        if (obj_pixel->color != DMG_WHITE && !bg_over_obj) // ignore transparent obj pixel and don't draw over another sprite (thus, respecting x priority)
            return obj_pixel;
        return bg_win_pixel;
    }
}

static inline void drawing_step(gb_t *gb) {
    gb_mmu_t *mmu = gb->mmu;
    gb_ppu_t *ppu = gb->ppu;

    // 1. FETCHING PIXELS

    switch (ppu->pixel_fetcher.step++) {
    case GET_TILE_ID:
        fetch_tile_id(gb);
        break;
    case GET_TILE_SLICE_LOW:
        fetch_tileslice_low(gb);
        break;
    case GET_TILE_SLICE_HIGH:
        fetch_tileslice_high(gb);
        if (!ppu->pixel_fetcher.init_delay_done) {
            ppu->pixel_fetcher.init_delay_done = 1;
            ppu->pixel_fetcher.step = 0;
        }
        break;
    case PUSH:
        ppu->pixel_fetcher.step = push(gb) ? 0 : PUSH;
        break;
    }

    if (IS_PPU_WIN_ENABLED(mmu) && mmu->io_registers[WY - IO] <= mmu->io_registers[LY - IO] && mmu->io_registers[WX - IO] - 7 <= ppu->lcd_x && ppu->pixel_fetcher.mode == FETCH_BG) {
        // the fetcher must switch to window mode for the remaining of the scanline (except when an object needs to be drawn)
        ppu->pixel_fetcher.mode = FETCH_WIN;
        ppu->pixel_fetcher.old_mode = FETCH_WIN;
        ppu->pixel_fetcher.step = 0;
        ppu->pixel_fetcher.x = 0;
        ppu->wly++;
        ppu->discarded_pixels = 0;
        pixel_fifo_clear(&ppu->bg_win_fifo);
    }

    if (ppu->bg_win_fifo.size > 0 && ppu->pixel_fetcher.mode != FETCH_OBJ && ppu->oam_scan.index < ppu->oam_scan.size && ppu->oam_scan.objs[ppu->oam_scan.index].x <= ppu->lcd_x + 8) {
        if (IS_PPU_OBJ_ENABLED(mmu)) {
            // there is an obj at the new position
            ppu->pixel_fetcher.mode = FETCH_OBJ;
            ppu->pixel_fetcher.step = 0;
            ppu->pixel_fetcher.curr_oam_index = ppu->oam_scan.index;
        } else {
            // TODO unsure of what to do in this case...
            //      should we allow a check for window?
            // TODO in CGB mode, this should also trigger a FETCH_OBJ but the pixels won't show on the lcd
        }
        ppu->oam_scan.index++;
    }

    // 2. SHIFTING PIXELS OUT TO THE LCD

    if (ppu->pixel_fetcher.mode == FETCH_OBJ)
        return;

    gb_pixel_t *bg_win_pixel = pixel_fifo_pop(&ppu->bg_win_fifo);
    if (!bg_win_pixel)
        return;

    // discards pixels if needed, due either to SCX or window starting before screen (WX < 7)
    switch (ppu->pixel_fetcher.mode) {
    case FETCH_BG:
        if (ppu->discarded_pixels < mmu->io_registers[SCX - IO] % 8) {
            ppu->discarded_pixels++;
            return;
        }
        break;
    case FETCH_WIN:
        if (mmu->io_registers[WX - IO] < 7 && ppu->discarded_pixels < 7 - mmu->io_registers[WX - IO]) {
            ppu->discarded_pixels++;
            return;
        }
        break;
    default:
        break;
    }

    gb_pixel_t *obj_pixel = pixel_fifo_pop(&ppu->obj_fifo);
    gb_pixel_t *selected_pixel = select_pixel(gb, bg_win_pixel, obj_pixel);
    byte_t selected_is_obj = selected_pixel == obj_pixel;

    if (gb->mode == GB_MODE_CGB) {
        if (IS_CGB_COMPAT_MODE(mmu))
            selected_pixel->color = dmg_get_color(mmu, selected_pixel, selected_is_obj);

        byte_t r, g, b;
        cgb_get_color(mmu, selected_pixel, selected_is_obj, !gb->disable_cgb_color_correction, &r, &g, &b);

        SET_PIXEL_CGB(gb, ppu->lcd_x, mmu->io_registers[LY - IO], r, g, b);
    } else {
        SET_PIXEL_DMG(gb, ppu->lcd_x, mmu->io_registers[LY - IO], dmg_get_color(mmu, selected_pixel, selected_is_obj));
    }
    ppu->lcd_x++;

    if (ppu->lcd_x >= GB_SCREEN_WIDTH) {
        // if (mmu->io_registers[LY - IO] == 0) {
        //     printf("%d in [172, 289]?\n", ppu->cycles - 80);
        // }
        // the new position is outside the screen, this scanline is done: go into HBLANK mode
        ppu->lcd_x = 0;

        reset_pixel_fetcher(ppu);

        pixel_fifo_clear(&ppu->bg_win_fifo);
        pixel_fifo_clear(&ppu->obj_fifo);

        ppu->oam_scan.index = 0;

        PPU_SET_MODE(mmu, PPU_MODE_HBLANK);
        if (IS_HBLANK_IRQ_STAT_ENABLED(gb))
            CPU_REQUEST_INTERRUPT(gb, IRQ_STAT);
        mmu->hdma.allow_hdma_block = mmu->hdma.type == HDMA && mmu->hdma.progress > 0;
    }
}

static inline void oam_scan_step(gb_t *gb) {
    gb_mmu_t *mmu = gb->mmu;
    gb_ppu_t *ppu = gb->ppu;

    // it takes 2 cycles to scan one OBJ so we skip every other step
    if (!ppu->oam_scan.step) {
        ppu->oam_scan.step = 1;
        return;
    }

    gb_obj_t *obj = (gb_obj_t *) &mmu->oam[ppu->oam_scan.index * 4];
    byte_t obj_height = IS_PPU_OBJ_TALL(mmu) ? 16 : 8;
    // NOTE: obj->x != 0 condition should not be checked even if ultimate gameboy talk says it should
    if (ppu->oam_scan.size < 10 && (obj->y <= mmu->io_registers[LY - IO] + 16) && (obj->y + obj_height > mmu->io_registers[LY - IO] + 16)) {
        s_byte_t i;
        // if equal x: insert after so that the drawing doesn't overwrite the existing sprite (equal x -> first scanned obj priority)
        for (i = ppu->oam_scan.size - 1; i >= 0 && ppu->oam_scan.objs[i].x > obj->x; i--) {
            ppu->oam_scan.objs[i + 1] = ppu->oam_scan.objs[i];
            ppu->oam_scan.objs_oam_pos[i + 1] = ppu->oam_scan.objs_oam_pos[i];
        }
        ppu->oam_scan.objs[i + 1] = *obj;
        ppu->oam_scan.objs_oam_pos[i + 1] = ppu->oam_scan.index;
        ppu->oam_scan.size++;
    }

    ppu->oam_scan.step = 0;
    ppu->oam_scan.index++;

    // if OAM scan has ended, switch to DRAWING mode
    if (ppu->oam_scan.index == 40) {
        ppu->oam_scan.step = 0;
        ppu->oam_scan.index = 0;

        ppu->discarded_pixels = 0;
        PPU_SET_MODE(mmu, PPU_MODE_DRAWING);
    }
}

static inline void hblank_step(gb_t *gb) {
    gb_mmu_t *mmu = gb->mmu;
    gb_ppu_t *ppu = gb->ppu;

    if (ppu->cycles < 456)
        return;

    mmu->io_registers[LY - IO]++;
    ppu->cycles = 0;

    if (mmu->io_registers[LY - IO] >= GB_SCREEN_HEIGHT) {
        ppu->wly = -1;
        PPU_SET_MODE(mmu, PPU_MODE_VBLANK);
        if (IS_VBLANK_IRQ_STAT_ENABLED(gb))
            CPU_REQUEST_INTERRUPT(gb, IRQ_STAT);

        CPU_REQUEST_INTERRUPT(gb, IRQ_VBLANK);

        // skip first screen rendering after the LCD was just turned on
        if (ppu->is_lcd_turning_on) {
            ppu->is_lcd_turning_on = 0;
            return;
        }

        if (gb->on_new_frame)
            gb->on_new_frame(ppu->pixels);
    } else {
        ppu->oam_scan.size = 0;
        PPU_SET_MODE(mmu, PPU_MODE_OAM);
        if (IS_OAM_IRQ_STAT_ENABLED(gb))
            CPU_REQUEST_INTERRUPT(gb, IRQ_STAT);
    }
}

static inline void vblank_step(gb_t *gb) {
    gb_mmu_t *mmu = gb->mmu;
    gb_ppu_t *ppu = gb->ppu;

    if (ppu->cycles == 4) {
        if (mmu->io_registers[LY - IO] == 153) {
            mmu->io_registers[LY - IO] = 0;
            ppu->is_last_vblank_line = 1;
        }
    } else if (ppu->cycles == 12) {
        if (ppu->is_last_vblank_line)
            ppu_ly_lyc_compare(gb);
    } else if (ppu->cycles == 456) {
        if (!ppu->is_last_vblank_line)
            mmu->io_registers[LY - IO]++; // increase on each new line
        ppu->cycles = 0;

        if (ppu->is_last_vblank_line) { // we actually are on line 153 but LY reads 0
            ppu->is_last_vblank_line = 0;

            ppu->oam_scan.size = 0;
            PPU_SET_MODE(mmu, PPU_MODE_OAM);
            if (IS_OAM_IRQ_STAT_ENABLED(gb))
                CPU_REQUEST_INTERRUPT(gb, IRQ_STAT);
        }
    }
}

static inline void blank_screen(gb_t *gb) {
    for (int x = 0; x < GB_SCREEN_WIDTH; x++) {
        for (int y = 0; y < GB_SCREEN_HEIGHT; y++) {
            if (gb->mode == GB_MODE_CGB) {
                byte_t r, g, b;
                cgb_get_color(gb->mmu, NULL, 0, !gb->disable_cgb_color_correction, &r, &g, &b);
                SET_PIXEL_CGB(gb, x, y, r, g, b);
            } else {
                SET_PIXEL_DMG(gb, x, y, DMG_WHITE);
            }
        }
    }
}

void ppu_step(gb_t *gb) {
    gb_ppu_t *ppu = gb->ppu;
    gb_mmu_t *mmu = gb->mmu;

    if (!IS_LCD_ENABLED(gb)) {
        // TODO not sure of the handling of LCD disabled
        // TODO LCD disabled should fill screen with a color brighter than WHITE

        if (!ppu->is_lcd_turning_on) {
            blank_screen(gb);

            if (gb->on_new_frame)
                gb->on_new_frame(ppu->pixels);
            ppu->is_lcd_turning_on = 1;

            PPU_SET_MODE(mmu, PPU_MODE_HBLANK);
            mmu->hdma.allow_hdma_block = mmu->hdma.type == HDMA && mmu->hdma.progress > 0;
            RESET_BIT(mmu->io_registers[STAT - IO], 2); // set LYC=LY to 0?
            ppu->wly = -1;
            reset_pixel_fetcher(ppu);
            pixel_fifo_clear(&ppu->bg_win_fifo);
            pixel_fifo_clear(&ppu->obj_fifo);
            ppu->cycles = 8; // TODO for some reason values from 5 to 8 (included) are needed to pass some tests (like blargg/oam_bug/rom_singles/1-lcd_sync.gb)...
            mmu->io_registers[LY - IO] = 0;
        }
        return;
    }

    // check for LYC=LY at the 4th cycle or if lcd was just enabled
    if (ppu->cycles == 4 || ppu->is_lcd_turning_on)
        ppu_ly_lyc_compare(gb);

    for (byte_t cycles = 0; cycles < 4; cycles++) { // 4 cycles per step
        switch (PPU_GET_MODE(gb)) {
        case PPU_MODE_OAM:
            // Scanning OAM for (X, Y) coordinates of sprites that overlap this line
            // there is up to 40 OBJs in OAM memory and this mode takes 80 cycles: 2 cycles per OBJ to check if it should be displayed
            // put them into an array of max size 10.
            oam_scan_step(gb);
            break;
        case PPU_MODE_DRAWING:
            // Reading OAM and VRAM to generate the picture
            drawing_step(gb);
            break;
        case PPU_MODE_HBLANK:
            // Horizontal blanking
            hblank_step(gb);
            break;
        case PPU_MODE_VBLANK:
            // Vertical blanking
            vblank_step(gb);
            break;
        }

        ppu->cycles++;
    }
}

void ppu_init(gb_t *gb) {
    gb->ppu = xcalloc(1, sizeof(gb_ppu_t));
    gb->ppu->wly = -1;
    // PPU_SET_MODE(gb->mmu, PPU_MODE_OAM); // TODO start in OAM mode?
}

void ppu_quit(gb_t *gb) {
    free(gb->ppu);
}

#define SERIALIZED_OAM_SCAN      \
    Y(oam_scan.objs, y)          \
    Y(oam_scan.objs, x)          \
    Y(oam_scan.objs, tile_id)    \
    Y(oam_scan.objs, attributes) \
    X(oam_scan.objs_oam_pos)     \
    X(oam_scan.size)             \
    X(oam_scan.index)            \
    X(oam_scan.step)

#define SERIALIZED_FIFO(fifo)  \
    Y(fifo.pixels, color)      \
    Y(fifo.pixels, palette)    \
    Y(fifo.pixels, attributes) \
    Y(fifo.pixels, oam_pos)    \
    X(fifo.size)               \
    X(fifo.head)               \
    X(fifo.tail)

#define SERIALIZED_FETCHER              \
    Y(pixel_fetcher.pixels, color)      \
    Y(pixel_fetcher.pixels, palette)    \
    Y(pixel_fetcher.pixels, attributes) \
    Y(pixel_fetcher.pixels, oam_pos)    \
    X(pixel_fetcher.mode)               \
    X(pixel_fetcher.old_mode)           \
    X(pixel_fetcher.step)               \
    X(pixel_fetcher.curr_oam_index)     \
    X(pixel_fetcher.init_delay_done)    \
    X(pixel_fetcher.current_tile_id)    \
    X(pixel_fetcher.x)

#define SERIALIZED_MEMBERS       \
    X(wly)                       \
    X(is_lcd_turning_on)         \
    X(cycles)                    \
    X(discarded_pixels)          \
    X(lcd_x)                     \
    X(wly)                       \
    X(is_last_vblank_line)       \
    SERIALIZED_OAM_SCAN          \
    SERIALIZED_FIFO(bg_win_fifo) \
    SERIALIZED_FIFO(obj_fifo)    \
    SERIALIZED_FETCHER

#define X(value) SERIALIZED_LENGTH(value);
#define Y(array, value) SERIALIZED_LENGTH_ARRAY_OF_STRUCTS(array, value);
SERIALIZED_SIZE_FUNCTION(gb_ppu_t, ppu,
    SERIALIZED_MEMBERS
)
#undef X
#undef Y

#define X(value) SERIALIZE(value);
#define Y(array, value) SERIALIZE_ARRAY_OF_STRUCTS(array, value);
SERIALIZER_FUNCTION(gb_ppu_t, ppu,
    SERIALIZED_MEMBERS
)
#undef X
#undef Y

#define X(value) UNSERIALIZE(value);
#define Y(array, value) UNSERIALIZE_ARRAY_OF_STRUCTS(array, value);
UNSERIALIZER_FUNCTION(gb_ppu_t, ppu,
    SERIALIZED_MEMBERS
)
#undef X
#undef Y