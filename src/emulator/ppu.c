#include <string.h>
#include <stdlib.h>

#include "emulator_priv.h"
#include "serialize.h"

// Useful resources
// https://pixelbits.16-b.it/GBEDG/ppu/
// https://github.com/gbdev/pandocs/blob/bae52a72501a4ae872cf06ee0b26b3a83b274fca/src/Rendering_Internals.md
// https://github.com/trekawek/coffee-gb/tree/master/src/main/java/eu/rekawek/coffeegb/gpu

// TODO read this: http://gameboy.mongenel.com/dmg/istat98.txt
// TODO read this for fetcher timings: https://www.reddit.com/r/EmuDev/comments/s6cpis/comment/htlwkx9
// TODO GBC

#define PPU_SET_MODE(mmu_ptr, mode) (mmu_ptr)->mem[STAT] = ((mmu_ptr)->mem[STAT] & 0xFC) | (mode)

#define IS_PPU_WIN_ENABLED(mmu_ptr) (CHECK_BIT((mmu_ptr)->mem[LCDC], 5))
#define IS_PPU_OBJ_TALL(mmu_ptr) (CHECK_BIT((mmu_ptr)->mem[LCDC], 2))
#define IS_PPU_OBJ_ENABLED(mmu_ptr) (CHECK_BIT((mmu_ptr)->mem[LCDC], 1))
#define IS_PPU_BG_WIN_ENABLED(mmu_ptr) (CHECK_BIT((mmu_ptr)->mem[LCDC], 0))

#define IS_PPU_BG_NORMAL_ADDRESS(mmu_ptr) CHECK_BIT((mmu_ptr)->mem[LCDC], 4)

#define SET_PIXEL_DMG(emu_ptr, x, y, color)                                                                                       \
    do {                                                                                                                          \
        *((emu_ptr)->ppu->pixels + ((y) *GB_SCREEN_WIDTH * 3) + ((x) *3)) = dmg_palettes[(emu_ptr)->dmg_palette][(color)][0];     \
        *((emu_ptr)->ppu->pixels + ((y) *GB_SCREEN_WIDTH * 3) + ((x) *3) + 1) = dmg_palettes[(emu_ptr)->dmg_palette][(color)][1]; \
        *((emu_ptr)->ppu->pixels + ((y) *GB_SCREEN_WIDTH * 3) + ((x) *3) + 2) = dmg_palettes[(emu_ptr)->dmg_palette][(color)][2]; \
    } while (0)

#define SET_PIXEL_CGB(emu_ptr, x, y, r, g, b)                                        \
    do {                                                                             \
        *((emu_ptr)->ppu->pixels + ((y) *GB_SCREEN_WIDTH * 3) + ((x) *3)) = (r);     \
        *((emu_ptr)->ppu->pixels + ((y) *GB_SCREEN_WIDTH * 3) + ((x) *3) + 1) = (g); \
        *((emu_ptr)->ppu->pixels + ((y) *GB_SCREEN_WIDTH * 3) + ((x) *3) + 2) = (b); \
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

void ppu_ly_lyc_compare(emulator_t *emu) {
    mmu_t *mmu = emu->mmu;

    if (mmu->mem[LY] == mmu->mem[LYC]) {
        SET_BIT(mmu->mem[STAT], 2);
        if (IS_LY_LYC_IRQ_STAT_ENABLED(emu))
            CPU_REQUEST_INTERRUPT(emu, IRQ_STAT);
    } else {
        RESET_BIT(mmu->mem[STAT], 2);
    }
}

// TODO if the ppu vram access is blocked, the tile id, tile map, etc. reads are 0xFF
static inline word_t get_bg_tiledata_address(emulator_t *emu, byte_t is_high) {
    byte_t bit_12 = !((emu->mmu->mem[LCDC] & 0x10) || (emu->ppu->pixel_fetcher.current_tile_id & 0x80));
    byte_t ly_scy = emu->mmu->mem[LY] + emu->mmu->mem[SCY]; // addition carried out in 8 bits (== result % 256)
    return 0x8000 | (bit_12 << 12) | (emu->ppu->pixel_fetcher.current_tile_id << 4) | ((ly_scy % 8) << 1) | !!is_high;
}

static inline word_t get_win_tiledata_address(emulator_t *emu, byte_t is_high) {
    byte_t bit_12 = !((emu->mmu->mem[LCDC] & 0x10) || (emu->ppu->pixel_fetcher.current_tile_id & 0x80));
    return 0x8000 | (bit_12 << 12) | (emu->ppu->pixel_fetcher.current_tile_id << 4) | ((emu->ppu->wly % 8) << 1) | !!is_high;
}

static inline word_t get_obj_tiledata_address(emulator_t *emu, byte_t is_high) {
    mmu_t *mmu = emu->mmu;
    ppu_t *ppu = emu->ppu;

    obj_t obj = ppu->oam_scan.objs[ppu->pixel_fetcher.curr_oam_index];

    byte_t flip_y = CHECK_BIT(obj.attributes, 6);
    byte_t actual_tile_id = obj.tile_id;

    if (IS_PPU_OBJ_TALL(mmu)) {
        byte_t is_top_tile = abs(mmu->mem[LY] - obj.y) > 8;
        CHANGE_BIT(actual_tile_id, 0, flip_y ? is_top_tile : !is_top_tile);
    }

    byte_t bits_3_1 = (mmu->mem[LY] - obj.y) % 8;
    if (flip_y)
        bits_3_1 = ~bits_3_1;
    bits_3_1 &= 0x07;

    return 0x8000 | (actual_tile_id << 4) | (bits_3_1 << 1) | !!is_high;
}

static inline pixel_t *pixel_fifo_pop(pixel_fifo_t *fifo) {
    if (fifo->size == 0)
        return NULL;
    fifo->size--;
    byte_t old_head = fifo->head;
    fifo->head = (fifo->head + 1) % PIXEL_FIFO_SIZE;
    return &fifo->pixels[old_head];
}

static inline void pixel_fifo_push(pixel_fifo_t *fifo, pixel_t *pixel) {
    if (fifo->size == PIXEL_FIFO_SIZE)
        return;
    fifo->size++;
    fifo->pixels[fifo->tail] = *pixel;
    fifo->tail = (fifo->tail + 1) % PIXEL_FIFO_SIZE;
}

static inline void pixel_fifo_clear(pixel_fifo_t *fifo) {
    fifo->size = 0;
    fifo->head = 0;
    fifo->tail = 0;
}

static inline void reset_pixel_fetcher(ppu_t *ppu) {
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

static inline void fetch_tile_id(emulator_t *emu) {
    mmu_t *mmu = emu->mmu;
    ppu_t *ppu = emu->ppu;

    switch (ppu->pixel_fetcher.mode) {
        case FETCH_BG: {
            byte_t ly_scy = mmu->mem[LY] + mmu->mem[SCY]; // addition carried out in 8 bits (== result % 256)
            byte_t fetcher_x_scx = ppu->pixel_fetcher.x + mmu->mem[SCX]; // addition carried out in 8 bits (== result % 256)
            word_t tilemap_address = 0x9800 | (GET_BIT(mmu->mem[LCDC], 3) << 10) | ((ly_scy / 8) << 5) | ((fetcher_x_scx / 8) & 0x1F);
            ppu->pixel_fetcher.current_tile_id = mmu->mem[tilemap_address];
            break;
        }
        case FETCH_WIN: {
            word_t tilemap_address = 0x9800 | (GET_BIT(mmu->mem[LCDC], 6) << 10) | (((emu->ppu->wly / 8) & 0x1F) << 5) | ((ppu->pixel_fetcher.x / 8) & 0x1F);
            ppu->pixel_fetcher.current_tile_id = mmu->mem[tilemap_address];
            break;
        }
        case FETCH_OBJ: {
            obj_t obj = ppu->oam_scan.objs[ppu->pixel_fetcher.curr_oam_index];
            ppu->pixel_fetcher.current_tile_id = obj.tile_id;
            break;
        }
    }
}

static inline void fetch_tileslice_low(emulator_t *emu) {
    mmu_t *mmu = emu->mmu;
    ppu_t *ppu = emu->ppu;

    byte_t tileslice = 0;
    switch (ppu->pixel_fetcher.mode) {
    case FETCH_BG:
        // if bg/win is enabled, get color data, else get 0
        tileslice = IS_PPU_BG_WIN_ENABLED(mmu) ? mmu->mem[get_bg_tiledata_address(emu, 0)] : 0;
        break;
    case FETCH_WIN:
        // if bg/win is enabled, get color data, else get 0
        tileslice = IS_PPU_BG_WIN_ENABLED(mmu) ? mmu->mem[get_bg_tiledata_address(emu, 0)] : 0;
        break;
    case FETCH_OBJ:
        word_t tiledata_address = get_obj_tiledata_address(emu, 0);
        byte_t flip_x = CHECK_BIT(ppu->oam_scan.objs[ppu->pixel_fetcher.curr_oam_index].attributes, 5);
        tileslice = flip_x ? reverse_bits_order(mmu->mem[tiledata_address]) : mmu->mem[tiledata_address];
        break;
    }

    for (byte_t i = 0; i < 8; i++)
        ppu->pixel_fetcher.pixels[i].color = GET_BIT(tileslice, 7 - i);
}

static inline void fetch_tileslice_high(emulator_t *emu) {
    mmu_t *mmu = emu->mmu;
    ppu_t *ppu = emu->ppu;

    byte_t tileslice = 0;
    // this recalculate the tiledata address because the ppu doesn't cache this info between steps,
    // leading to potential bitplane desync
    switch (ppu->pixel_fetcher.mode) {
    case FETCH_BG:
        // if bg/win is enabled, get color data, else get 0
        tileslice = IS_PPU_BG_WIN_ENABLED(mmu) ? mmu->mem[get_bg_tiledata_address(emu, 1)] : 0;
        for (byte_t i = 0; i < 8; i++) {
            ppu->pixel_fetcher.pixels[i].color |= GET_BIT(tileslice, 7 - i) << 1;
            ppu->pixel_fetcher.pixels[i].palette = BGP;
        }
        break;
    case FETCH_WIN:
        // if bg/win is enabled, get color data, else get 0
        tileslice = IS_PPU_BG_WIN_ENABLED(mmu) ? mmu->mem[get_win_tiledata_address(emu, 10)] : 0;
        for (byte_t i = 0; i < 8; i++) {
            ppu->pixel_fetcher.pixels[i].color |= GET_BIT(tileslice, 7 - i) << 1;
            ppu->pixel_fetcher.pixels[i].palette = BGP;
        }
        break;
    case FETCH_OBJ:
        word_t  tiledata_address = get_obj_tiledata_address(emu, 1);

        byte_t flip_x = CHECK_BIT(ppu->oam_scan.objs[ppu->pixel_fetcher.curr_oam_index].attributes, 5);
        tileslice = flip_x ? reverse_bits_order(mmu->mem[tiledata_address]) : mmu->mem[tiledata_address];

        for (byte_t i = 0; i < 8; i++) {
            ppu->pixel_fetcher.pixels[i].color |= GET_BIT(tileslice, 7 - i) << 1;
            ppu->pixel_fetcher.pixels[i].palette = CHECK_BIT(ppu->oam_scan.objs[ppu->pixel_fetcher.curr_oam_index].attributes, 4) ? OBP1 : OBP0;
            ppu->pixel_fetcher.pixels[i].bg_priority = CHECK_BIT(ppu->oam_scan.objs[ppu->pixel_fetcher.curr_oam_index].attributes, 7);
        }
        break;
    }
}

static inline byte_t push(emulator_t *emu) {
    ppu_t *ppu = emu->ppu;

    switch (ppu->pixel_fetcher.mode) {
    case FETCH_BG:
    case FETCH_WIN:
        if (ppu->bg_win_fifo.size > 0)
            return 0;

        for (byte_t i = 0; i < 8; i++)
            pixel_fifo_push(&ppu->bg_win_fifo, &ppu->pixel_fetcher.pixels[i]);
        ppu->pixel_fetcher.x += 8;
        break;
    case FETCH_OBJ:
        // if the obj starts before the beginning of the scanline, only push the visible pixels by using an offset
        byte_t old_size = ppu->obj_fifo.size;
        byte_t offset = 0;
        if (ppu->oam_scan.objs[ppu->pixel_fetcher.curr_oam_index].x < 8)
            offset = 8 - ppu->oam_scan.objs[ppu->pixel_fetcher.curr_oam_index].x;

        // overwrite old transparent obj pixels, then fill rest of fifo with the last pixels of this obj to not overwrite any old pixels
        for (byte_t fetcher_index = offset; fetcher_index < PIXEL_FIFO_SIZE; fetcher_index++) {
            byte_t fifo_index = ((fetcher_index - offset) + ppu->obj_fifo.head) % PIXEL_FIFO_SIZE;

            if (fetcher_index < old_size) {
                if (ppu->obj_fifo.pixels[fifo_index].color == DMG_WHITE)
                    ppu->obj_fifo.pixels[fifo_index] = ppu->pixel_fetcher.pixels[fetcher_index];
            } else {
                pixel_fifo_push(&ppu->obj_fifo, &ppu->pixel_fetcher.pixels[fetcher_index]);
            }
        }

        ppu->pixel_fetcher.mode = ppu->pixel_fetcher.old_mode;
        break;
    }

    return 1;
}

static inline void drawing_step(emulator_t *emu) {
    mmu_t *mmu = emu->mmu;
    ppu_t *ppu = emu->ppu;

    // 1. FETCHING PIXELS

    switch (ppu->pixel_fetcher.step++) {
    case GET_TILE_ID:
        fetch_tile_id(emu);
        break;
    case GET_TILE_SLICE_LOW:
        fetch_tileslice_low(emu);
        break;
    case GET_TILE_SLICE_HIGH:
        fetch_tileslice_high(emu);
        if (!ppu->pixel_fetcher.init_delay_done) {
            ppu->pixel_fetcher.init_delay_done = 1;
            ppu->pixel_fetcher.step = 0;
        }
        break;
    case PUSH:
        ppu->pixel_fetcher.step = push(emu) ? 0 : PUSH;
        break;
    }

    if (IS_PPU_WIN_ENABLED(mmu) && mmu->mem[WY] <= mmu->mem[LY] && mmu->mem[WX] - 7 <= ppu->lcd_x && ppu->pixel_fetcher.mode == FETCH_BG) {
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
        }
        ppu->oam_scan.index++;
    }

    // 2. SHIFTING PIXELS OUT TO THE LCD

    if (ppu->pixel_fetcher.mode == FETCH_OBJ)
        return;

    pixel_t *bg_win_pixel = pixel_fifo_pop(&ppu->bg_win_fifo);
    if (!bg_win_pixel)
        return;

    if ((ppu->pixel_fetcher.mode == FETCH_WIN && ppu->discarded_pixels < 7 - mmu->mem[WX]) || ppu->discarded_pixels < mmu->mem[SCX] % 8) {
        ppu->discarded_pixels++;
        return;
    }

    byte_t color = bg_win_pixel->color;
    word_t palette = bg_win_pixel->palette;

    // merge pixels
    pixel_t *obj_pixel = pixel_fifo_pop(&ppu->obj_fifo);
    if (obj_pixel) {
        byte_t bg_over_obj = obj_pixel->bg_priority && bg_win_pixel->color != DMG_WHITE;
        if (obj_pixel->color != DMG_WHITE && !bg_over_obj) { // ignore transparent obj pixel and don't draw over another sprite (thus, respecting x priority)
            color = obj_pixel->color;
            palette = obj_pixel->palette;
        }
    }

    SET_PIXEL_DMG(emu, ppu->lcd_x, mmu->mem[LY], get_color(mmu, color, palette));
    ppu->lcd_x++;

    if (ppu->lcd_x >= GB_SCREEN_WIDTH) {
        // if (mmu->mem[LY] == 0) {
        //     printf("%d in [172, 289]?\n", ppu->cycles - 80);
        // }
        // the new position is outside the screen, this scanline is done: go into HBLANK mode
        ppu->lcd_x = 0;

        reset_pixel_fetcher(ppu);

        pixel_fifo_clear(&ppu->bg_win_fifo);
        pixel_fifo_clear(&ppu->obj_fifo);

        ppu->oam_scan.index = 0;

        PPU_SET_MODE(mmu, PPU_MODE_HBLANK);
        if (IS_HBLANK_IRQ_STAT_ENABLED(emu))
            CPU_REQUEST_INTERRUPT(emu, IRQ_STAT);
    }
}

static inline void oam_scan_step(emulator_t *emu) {
    mmu_t *mmu = emu->mmu;
    ppu_t *ppu = emu->ppu;

    // it takes 2 cycles to scan one OBJ so we skip every other step
    if (!ppu->oam_scan.step) {
        ppu->oam_scan.step = 1;
        return;
    }

    obj_t *obj = (obj_t *) &mmu->mem[OAM + (ppu->oam_scan.index * 4)];
    byte_t obj_height = IS_PPU_OBJ_TALL(mmu) ? 16 : 8;
    // NOTE: obj->x != 0 condition should not be checked even if ultimate gameboy talk says it should
    if (ppu->oam_scan.size < 10 && (obj->y <= mmu->mem[LY] + 16) && (obj->y + obj_height > mmu->mem[LY] + 16)) {
        s_byte_t i;
        // if equal x: insert after so that the drawing doesn't overwrite the existing sprite (equal x -> first scanned obj priority)
        for (i = ppu->oam_scan.size - 1; i >= 0 && ppu->oam_scan.objs[i].x > obj->x; i--)
            ppu->oam_scan.objs[i + 1] = ppu->oam_scan.objs[i];
        ppu->oam_scan.objs[i + 1] = *obj;
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

static inline void hblank_step(emulator_t *emu) {
    mmu_t *mmu = emu->mmu;
    ppu_t *ppu = emu->ppu;

    if (ppu->cycles == 456) {
        mmu->mem[LY]++;

        if (mmu->mem[LY] >= GB_SCREEN_HEIGHT) {
            ppu->cycles = 0;
            ppu->wly = -1;
            PPU_SET_MODE(mmu, PPU_MODE_VBLANK);
            if (IS_VBLANK_IRQ_STAT_ENABLED(emu))
                CPU_REQUEST_INTERRUPT(emu, IRQ_STAT);

            CPU_REQUEST_INTERRUPT(emu, IRQ_VBLANK);

            // skip first screen rendering after the LCD was just turned on
            if (ppu->is_lcd_turning_on) {
                ppu->is_lcd_turning_on = 0;
                return;
            }

            if (emu->on_new_frame)
                emu->on_new_frame(ppu->pixels);
        } else {
            ppu->oam_scan.size = 0;
            ppu->cycles = 0;
            PPU_SET_MODE(mmu, PPU_MODE_OAM);
            if (IS_OAM_IRQ_STAT_ENABLED(emu))
                CPU_REQUEST_INTERRUPT(emu, IRQ_STAT);
        }
    }
}

static inline void vblank_step(emulator_t *emu) {
    mmu_t *mmu = emu->mmu;
    ppu_t *ppu = emu->ppu;

    if (ppu->cycles == 4) {
        if (mmu->mem[LY] == 153) {
            mmu->mem[LY] = 0;
            ppu->is_last_vblank_line = 1;
        }
    } else if (ppu->cycles == 12) {
        if (ppu->is_last_vblank_line)
            ppu_ly_lyc_compare(emu);
    } else if (ppu->cycles == 456) {
        if (!ppu->is_last_vblank_line)
            mmu->mem[LY]++; // increase on each new line
        ppu->cycles = 0;

        if (ppu->is_last_vblank_line) { // we actually are on line 153 but LY reads 0
            ppu->is_last_vblank_line = 0;

            ppu->oam_scan.size = 0;
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
                    SET_PIXEL_DMG(emu, i, j, DMG_WHITE);

            if (emu->on_new_frame)
                emu->on_new_frame(ppu->pixels);
            ppu->is_lcd_turning_on = 1;

            PPU_SET_MODE(mmu, PPU_MODE_HBLANK);
            RESET_BIT(mmu->mem[STAT], 2); // set LYC=LY to 0?
            ppu->wly = -1;
            reset_pixel_fetcher(ppu);
            pixel_fifo_clear(&ppu->bg_win_fifo);
            pixel_fifo_clear(&ppu->obj_fifo);
            ppu->cycles = 0; //369; // (== 456 - 87) set hblank duration to its minimum possible value
            mmu->mem[LY] = 0;
        }
        return;
    }

    // if lcd was just enabled, check for LYC=LY
    if (ppu->is_lcd_turning_on)
        ppu_ly_lyc_compare(emu);

    if (ppu->cycles == 4)
        ppu_ly_lyc_compare(emu);

    int cycles = 4;
    while (cycles--) {
        switch (PPU_GET_MODE(emu)) {
        case PPU_MODE_OAM:
            // Scanning OAM for (X, Y) coordinates of sprites that overlap this line
            // there is up to 40 OBJs in OAM memory and this mode takes 80 cycles: 2 cycles per OBJ to check if it should be displayed
            // put them into an array of max size 10.
            oam_scan_step(emu);
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

        ppu->cycles++;
    }
}

void ppu_init(emulator_t *emu) {
    emu->ppu = xcalloc(1, sizeof(ppu_t));
    emu->ppu->wly = -1;
    // PPU_SET_MODE(emu->mmu, PPU_MODE_OAM); // TODO start in OAM mode?
    emu->ppu->pixels = xmalloc(GB_SCREEN_WIDTH * GB_SCREEN_HEIGHT * 4);
}

void ppu_quit(emulator_t *emu) {
    free(emu->ppu->pixels);
    free(emu->ppu);
}

#define SERIALIZED_MEMBERS           \
    X(wly)                           \
    X(is_lcd_turning_on)             \
    X(cycles)                        \
    X(discarded_pixels)              \
    X(lcd_x)                         \
    X(wly)                           \
    X(is_last_vblank_line)           \
    X(oam_scan.objs)                 \
    X(oam_scan.size)                 \
    X(oam_scan.index)                \
    X(oam_scan.step)                 \
    X(bg_win_fifo)                   \
    X(obj_fifo)                      \
    X(pixel_fetcher.pixels)          \
    X(pixel_fetcher.mode)            \
    X(pixel_fetcher.old_mode)        \
    X(pixel_fetcher.step)            \
    X(pixel_fetcher.curr_oam_index)  \
    X(pixel_fetcher.init_delay_done) \
    X(pixel_fetcher.current_tile_id) \
    X(pixel_fetcher.x)

#define X(value) SERIALIZED_LENGTH(value);
SERIALIZED_SIZE_FUNCTION(ppu_t, ppu,
    SERIALIZED_MEMBERS;
)
#undef X

#define X(value) SERIALIZE(value);
SERIALIZER_FUNCTION(ppu_t, ppu,
    SERIALIZED_MEMBERS
)
#undef X

#define X(value) UNSERIALIZE(value);
UNSERIALIZER_FUNCTION(ppu_t, ppu,
    SERIALIZED_MEMBERS
)
#undef X
