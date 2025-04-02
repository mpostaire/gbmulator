#include <string.h>
#include <stdlib.h>

#include "gb_priv.h"
#include "serialize.h"

// Useful resources
// https://hacktix.github.io/GBEDG/ppu/
// https://github.com/gbdev/pandocs/blob/bae52a72501a4ae872cf06ee0b26b3a83b274fca/src/Rendering_Internals.md
// https://github.com/trekawek/coffee-gb/tree/master/src/main/java/eu/rekawek/coffeegb/gpu

// TODO read this: http://gameboy.mongenel.com/dmg/istat98.txt
// TODO read this for fetcher timings: https://www.reddit.com/r/EmuDev/comments/s6cpis/comment/htlwkx9
// https://github.com/vojty/feather-gb and its debugger

#define SCANLINE_CYCLES 456

#define PPU_SET_STAT_MODE(gb, new_mode) ((gb)->mmu->io_registers[IO_STAT] = ((gb)->mmu->io_registers[IO_STAT] & 0xFC) | (new_mode))

#define IS_WIN_ENABLED(gb) (CHECK_BIT((gb)->mmu->io_registers[IO_LCDC], 5))
#define IS_OBJ_TALL(gb) (CHECK_BIT((gb)->mmu->io_registers[IO_LCDC], 2))
#define IS_OBJ_ENABLED(gb) (CHECK_BIT((gb)->mmu->io_registers[IO_LCDC], 1))
#define IS_BG_WIN_ENABLED(gb) (CHECK_BIT((gb)->mmu->io_registers[IO_LCDC], 0))

#define IS_CGB_COMPAT_MODE(gb) ((((gb)->mmu->io_registers[IO_KEY0] >> 2) & 0x03) == 1)

#define IS_INSIDE_WINDOW(gb) \
    (ppu->win_actually_enabled && (gb)->mmu->io_registers[IO_WY] <= (gb)->mmu->io_registers[IO_LY] && ppu->is_wx_triggered)

#define SET_PIXEL_DMG(gb, x, y, color)                                                                                \
    do {                                                                                                              \
        (gb)->ppu->pixels[((y) * GB_SCREEN_WIDTH * 4) + ((x) * 4)] = dmg_palettes[(gb)->dmg_palette][(color)][0];     \
        (gb)->ppu->pixels[((y) * GB_SCREEN_WIDTH * 4) + ((x) * 4) + 1] = dmg_palettes[(gb)->dmg_palette][(color)][1]; \
        (gb)->ppu->pixels[((y) * GB_SCREEN_WIDTH * 4) + ((x) * 4) + 2] = dmg_palettes[(gb)->dmg_palette][(color)][2]; \
        (gb)->ppu->pixels[((y) * GB_SCREEN_WIDTH * 4) + ((x) * 4) + 3] = 0xFF;                                        \
    } while (0)

#define SET_PIXEL_CGB(gb, x, y, r, g, b)                                       \
    do {                                                                       \
        (gb)->ppu->pixels[((y) * GB_SCREEN_WIDTH * 4) + ((x) * 4)] = (r);      \
        (gb)->ppu->pixels[((y) * GB_SCREEN_WIDTH * 4) + ((x) * 4) + 1] = (g);  \
        (gb)->ppu->pixels[((y) * GB_SCREEN_WIDTH * 4) + ((x) * 4) + 2] = (b);  \
        (gb)->ppu->pixels[((y) * GB_SCREEN_WIDTH * 4) + ((x) * 4) + 3] = 0xFF; \
    } while (0)

static const uint8_t dmg_palettes[PPU_COLOR_PALETTE_MAX][4][3] = {
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
 * This sets the ppu mode and STAT mode and handles stat irq line interrupts
 */
static inline void set_mode(gb_t *gb, uint8_t mode) {
    switch (mode) {
    case PPU_MODE_VBLANK:
        // if OAM stat irq is enabled while entering vblank, a stat irq can be requested depending on the stat irq line
        gb->ppu->mode = PPU_MODE_OAM;
        ppu_update_stat_irq_line(gb);
        gb->ppu->mode = mode;

        if (!gb->is_cgb) {
            // no delay to set STAT mode bits and request VBLANK irq
            PPU_SET_STAT_MODE(gb, mode);
            ppu_update_stat_irq_line(gb);
            CPU_REQUEST_INTERRUPT(gb, IRQ_VBLANK);
        } else {
            gb->ppu->pending_stat_mode = mode;
        }
        break;
    case PPU_MODE_HBLANK:
    case PPU_MODE_OAM:
    case PPU_MODE_DRAWING:
        gb->ppu->mode = mode;
        gb->ppu->pending_stat_mode = mode;
        break;
    }
}

/**
 * @returns dmg color after applying palette.
 */
static inline gb_dmg_color_t dmg_get_color(gb_mmu_t *mmu, gb_pixel_t *pixel, uint8_t is_obj) {
    // return the color using the palette
    uint16_t palette_address = is_obj ? IO_OBP0 + pixel->palette : IO_BGP;
    return (mmu->io_registers[palette_address] >> (pixel->color << 1)) & 0x03;
}

/**
 * @returns cgb colors in r, g, b arguments after applying palette.
 */
static inline void cgb_get_color(gb_mmu_t *mmu, gb_pixel_t *pixel, uint8_t is_obj, uint8_t enable_color_correction, uint8_t *r, uint8_t *g, uint8_t *b) {
    uint16_t color_data;
    if (pixel) {
        uint8_t color_address = pixel->palette * 8 + pixel->color * 2;
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

static inline uint8_t cgb_get_bg_win_tile_attributes(gb_t *gb) {
    gb_mmu_t *mmu = gb->mmu;
    gb_ppu_t *ppu = gb->ppu;

    uint16_t tile_id_address;
    switch (ppu->pixel_fetcher.mode) {
    case FETCH_BG: {
        uint8_t ly_scy = mmu->io_registers[IO_LY] + mmu->io_registers[IO_SCY]; // addition carried out in 8 bits (== result % 256)
        uint8_t fetcher_x_scx = ppu->pixel_fetcher.x + mmu->io_registers[IO_SCX]; // addition carried out in 8 bits (== result % 256)
        tile_id_address = 0x9800 | (GET_BIT(mmu->io_registers[IO_LCDC], 3) << 10) | ((ly_scy / 8) << 5) | ((fetcher_x_scx / 8) & 0x1F);
        break;
    }
    case FETCH_WIN:
        tile_id_address = 0x9800 | (GET_BIT(mmu->io_registers[IO_LCDC], 6) << 10) | (((ppu->wly / 8) & 0x1F) << 5) | ((ppu->pixel_fetcher.x / 8) & 0x1F);
        break;
    default:
        eprintf("this function can't be used for objs");
        exit(EXIT_FAILURE);
        break;
    }
    return mmu->vram[tile_id_address - MMU_VRAM + VRAM_BANK_SIZE]; // read inside VRAM bank 1
}

// TODO if the ppu vram access is blocked, the tile id, tile map, etc. reads are 0xFF
static inline uint16_t get_bg_tiledata_address(gb_t *gb, uint8_t is_high, uint8_t flip_y) {
    gb_mmu_t *mmu = gb->mmu;
    gb_ppu_t *ppu = gb->ppu;

    uint8_t bit_12 = !((mmu->io_registers[IO_LCDC] & 0x10) || (ppu->pixel_fetcher.current_tile_id & 0x80));
    uint8_t ly_scy = mmu->io_registers[IO_LY] + mmu->io_registers[IO_SCY]; // addition carried out in 8 bits (== result % 256)
    if (flip_y)
        ly_scy = ~ly_scy;
    ly_scy &= 0x07; // modulo 8
    return 0x8000 | (bit_12 << 12) | (ppu->pixel_fetcher.current_tile_id << 4) | (ly_scy << 1) | (!!is_high);
}

static inline uint16_t get_win_tiledata_address(gb_t *gb, uint8_t is_high, uint8_t flip_y) {
    gb_mmu_t *mmu = gb->mmu;
    gb_ppu_t *ppu = gb->ppu;

    uint8_t bit_12 = !((mmu->io_registers[IO_LCDC] & 0x10) || (ppu->pixel_fetcher.current_tile_id & 0x80));
    int16_t wly = ppu->wly;
    if (flip_y)
        wly = ~wly;
    wly &= 0x07; // modulo 8
    return 0x8000 | (bit_12 << 12) | (ppu->pixel_fetcher.current_tile_id << 4) | (wly << 1) | (!!is_high);
}

static inline uint16_t get_obj_tiledata_address(gb_t *gb, uint8_t is_high) {
    gb_mmu_t *mmu = gb->mmu;
    gb_ppu_t *ppu = gb->ppu;

    gb_obj_t obj = ppu->oam_scan.objs[ppu->pixel_fetcher.curr_oam_index];

    uint8_t flip_y = CHECK_BIT(obj.attributes, 6);
    uint8_t actual_tile_id = obj.tile_id;

    if (IS_OBJ_TALL(gb)) {
        uint8_t is_top_tile = abs(mmu->io_registers[IO_LY] - obj.y) > 8;
        CHANGE_BIT(actual_tile_id, 0, flip_y ? is_top_tile : !is_top_tile);
    }

    uint8_t bits_3_1 = (mmu->io_registers[IO_LY] - obj.y) % 8;
    if (flip_y)
        bits_3_1 = ~bits_3_1;
    bits_3_1 &= 0x07; // modulo 8

    return 0x8000 | (actual_tile_id << 4) | (bits_3_1 << 1) | (!!is_high);
}

static inline gb_pixel_t *pixel_fifo_pop(gb_pixel_fifo_t *fifo) {
    if (fifo->size == 0)
        return NULL;
    fifo->size--;
    uint8_t old_head = fifo->head;
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
    ppu->pixel_fetcher.old_mode = FETCH_BG;
    ppu->pixel_fetcher.is_dummy_fetch_done = 0;
    ppu->discarded_pixels = 0;
}

/**
 * taken from https://stackoverflow.com/a/2602885
 */
static inline uint8_t reverse_bits_order(uint8_t b) {
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
            uint8_t ly_scy = mmu->io_registers[IO_LY] + mmu->io_registers[IO_SCY]; // addition carried out in 8 bits (== result % 256)
            uint8_t fetcher_x_scx = ppu->pixel_fetcher.x + mmu->io_registers[IO_SCX]; // addition carried out in 8 bits (== result % 256)
            uint16_t tile_id_address = 0x9800 | (GET_BIT(mmu->io_registers[IO_LCDC], 3) << 10) | ((ly_scy / 8) << 5) | ((fetcher_x_scx / 8) & 0x1F);
            ppu->pixel_fetcher.current_tile_id = mmu->vram[tile_id_address - MMU_VRAM];
            break;
        }
        case FETCH_WIN: {
            uint16_t tile_id_address = 0x9800 | (GET_BIT(mmu->io_registers[IO_LCDC], 6) << 10) | (((gb->ppu->wly / 8) & 0x1F) << 5) | ((ppu->pixel_fetcher.x / 8) & 0x1F);
            ppu->pixel_fetcher.current_tile_id = mmu->vram[tile_id_address - MMU_VRAM];
            break;
        }
        case FETCH_OBJ: {
            gb_obj_t obj = ppu->oam_scan.objs[ppu->pixel_fetcher.curr_oam_index];
            ppu->pixel_fetcher.current_tile_id = obj.tile_id;
            break;
        }
    }
}

static inline uint8_t get_tileslice(gb_t *gb, uint16_t tiledata_address, uint8_t attributes) {
    gb_mmu_t *mmu = gb->mmu;

    uint8_t flip_x = CHECK_BIT(attributes, 5);
    if (gb->is_cgb) {
        if (CHECK_BIT(attributes, 3)) // tile is in VRAM bank 1
            return flip_x ? reverse_bits_order(mmu->vram[tiledata_address - MMU_VRAM + VRAM_BANK_SIZE]) : mmu->vram[tiledata_address - MMU_VRAM + VRAM_BANK_SIZE];
        else
            return flip_x ? reverse_bits_order(mmu->vram[tiledata_address - MMU_VRAM]) : mmu->vram[tiledata_address - MMU_VRAM];
    } else {
        return flip_x ? reverse_bits_order(mmu->vram[tiledata_address - MMU_VRAM]) : mmu->vram[tiledata_address - MMU_VRAM];
    }
}

static inline void fetch_tileslice_low(gb_t *gb) {
    gb_ppu_t *ppu = gb->ppu;

    uint8_t tileslice = 0;
    switch (ppu->pixel_fetcher.mode) {
        case FETCH_BG: {
            uint8_t attributes = gb->is_cgb ? cgb_get_bg_win_tile_attributes(gb) : 0;
            uint16_t tiledata_address = get_bg_tiledata_address(gb, 0, CHECK_BIT(attributes, 6));
            tileslice = get_tileslice(gb, tiledata_address, attributes);
            break;
        }
        case FETCH_WIN: {
            uint8_t attributes = gb->is_cgb ? cgb_get_bg_win_tile_attributes(gb) : 0;
            uint16_t tiledata_address = get_win_tiledata_address(gb, 0, CHECK_BIT(attributes, 6));
            tileslice = get_tileslice(gb, tiledata_address, attributes);
            break;
        }
        case FETCH_OBJ: {
            uint8_t attributes = ppu->oam_scan.objs[ppu->pixel_fetcher.curr_oam_index].attributes;
            uint16_t tiledata_address = get_obj_tiledata_address(gb, 0);
            tileslice = get_tileslice(gb, tiledata_address, attributes);
            break;
        }
    }

    for (uint8_t i = 0; i < 8; i++)
        ppu->pixel_fetcher.pixels[i].color = GET_BIT(tileslice, 7 - i);
}

static inline void fetch_tileslice_high(gb_t *gb) {
    gb_ppu_t *ppu = gb->ppu;

    uint8_t tileslice = 0;
    uint16_t palette = 0;
    uint8_t attributes = 0;
    uint8_t oam_pos = 0;
    // this recalculate the tiledata address because the ppu doesn't cache this info between steps,
    // leading to potential bitplane desync
    switch (ppu->pixel_fetcher.mode) {
        case FETCH_BG: {
            if (gb->is_cgb) {
                attributes = cgb_get_bg_win_tile_attributes(gb);
                palette = IS_CGB_COMPAT_MODE(gb) ? 0 : attributes & 0x07;
            } else {
                palette = 0;
            }
            uint16_t tiledata_address = get_bg_tiledata_address(gb, 1, CHECK_BIT(attributes, 6));
            tileslice = get_tileslice(gb, tiledata_address, attributes);
            break;
        }
        case FETCH_WIN: {
            if (gb->is_cgb) {
                attributes = cgb_get_bg_win_tile_attributes(gb);
                palette = IS_CGB_COMPAT_MODE(gb) ? 0 : attributes & 0x07;
            } else {
                palette = 0;
            }
            uint16_t tiledata_address = get_win_tiledata_address(gb, 1, CHECK_BIT(attributes, 6));
            tileslice = get_tileslice(gb, tiledata_address, attributes);
            break;
        }
        case FETCH_OBJ: {
            attributes = ppu->oam_scan.objs[ppu->pixel_fetcher.curr_oam_index].attributes;
            uint16_t tiledata_address = get_obj_tiledata_address(gb, 1);
            tileslice = get_tileslice(gb, tiledata_address, attributes);
            oam_pos = ppu->oam_scan.objs_oam_pos[ppu->pixel_fetcher.curr_oam_index];

            uint8_t is_cgb_mode = gb->is_cgb && !IS_CGB_COMPAT_MODE(gb);
            palette = is_cgb_mode ? attributes & 0x07 : GET_BIT(attributes, 4);
            break;
        }
    }

    for (uint8_t i = 0; i < 8; i++) {
        ppu->pixel_fetcher.pixels[i].color |= GET_BIT(tileslice, 7 - i) << 1;        
        ppu->pixel_fetcher.pixels[i].palette = palette;
        ppu->pixel_fetcher.pixels[i].attributes = attributes;
        ppu->pixel_fetcher.pixels[i].oam_pos = oam_pos;
    }
}

static inline uint8_t push(gb_t *gb) {
    gb_ppu_t *ppu = gb->ppu;

    switch (ppu->pixel_fetcher.mode) {
        case FETCH_BG:
        case FETCH_WIN:
            if (ppu->bg_win_fifo.size > 0)
                return 0;

            for (uint8_t i = 0; i < 8; i++)
                pixel_fifo_push(&ppu->bg_win_fifo, &ppu->pixel_fetcher.pixels[i]);
            ppu->pixel_fetcher.x += 8;
            break;
        case FETCH_OBJ: {
            // if the obj starts before the beginning of the scanline, only push the visible pixels by using an offset
            uint8_t old_size = ppu->obj_fifo.size;
            uint8_t offset = 0;
            if (ppu->oam_scan.objs[ppu->pixel_fetcher.curr_oam_index].x < 8)
                offset = 8 - ppu->oam_scan.objs[ppu->pixel_fetcher.curr_oam_index].x;

            uint8_t is_cgb_mode = gb->is_cgb && !CHECK_BIT(gb->mmu->io_registers[IO_OPRI], 0);
            for (uint8_t fetcher_index = offset; fetcher_index < PIXEL_FIFO_SIZE; fetcher_index++) {
                uint8_t fifo_index = ((fetcher_index - offset) + ppu->obj_fifo.head) % PIXEL_FIFO_SIZE;

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
    uint8_t is_cgb_mode = gb->is_cgb && !IS_CGB_COMPAT_MODE(gb);
    if (is_cgb_mode) {
        if (!obj_pixel || !IS_OBJ_ENABLED(gb) || obj_pixel->color == DMG_WHITE)
            return bg_win_pixel;
        if (bg_win_pixel->color == DMG_WHITE || !IS_BG_WIN_ENABLED(gb))
            return obj_pixel;
        if (!CHECK_BIT(bg_win_pixel->attributes, 7) && !CHECK_BIT(obj_pixel->attributes, 7))
            return obj_pixel;
        return bg_win_pixel;
    } else {
        if (!IS_BG_WIN_ENABLED(gb)) {
            bg_win_pixel->color = DMG_WHITE; // in some cases, this can overwrite color of glitched_pixel but glitched_pixel color is DMG_WHITE so it's not a problem
            // TODO keep palette / change palette / force white?
        }

        if (!obj_pixel)
            return bg_win_pixel;
        if (!IS_OBJ_ENABLED(gb))
            return bg_win_pixel;

        uint8_t bg_over_obj = CHECK_BIT(obj_pixel->attributes, 7) && bg_win_pixel->color != DMG_WHITE;
        if (obj_pixel->color != DMG_WHITE && !bg_over_obj) // ignore transparent obj pixel and don't draw over another sprite (thus, respecting x priority)
            return obj_pixel;
        return bg_win_pixel;
    }
}

// https://github.com/mattcurrie/mealybug-tearoom-tests/blob/70e88fb90b59d19dfbb9c3ac36c64105202bb1f4/the-comprehensive-game-boy-ppu-documentation.md#win_en-bit-5
static inline gb_pixel_t *handle_window(gb_t *gb) {
    static gb_pixel_t glitched_pixel = { 0 };

    gb_mmu_t *mmu = gb->mmu;
    gb_ppu_t *ppu = gb->ppu;

    // wait for the current window tile to finish rendering, then the fetcher mode switches to BG
    // --> ppu->pixel_fetcher.step == 0 means that previous 8 pixels were just pushed
    if (ppu->wly != -1 && !IS_WIN_ENABLED(gb) && ppu->pixel_fetcher.step == 0) {
        // window was disabled in the middle of a frame where it was already enabled
        ppu->saved_wly = ppu->wly;
        ppu->wly = -1;

        ppu->pixel_fetcher.old_mode = FETCH_BG;
        ppu->pixel_fetcher.mode = FETCH_BG;
        ppu->pixel_fetcher.step = 0;
    }

    // a glitched pixel can be drawn if the window is disabled but it was enabled before in the same frame and we are on DMG
    // https://github.com/LIJI32/SameBoy/issues/278#issuecomment-1189712129
    // https://github.com/shonumi/gbe-plus/commit/15df53c83677062f98915293fc03620af65bd7c4
    // https://www.reddit.com/r/EmuDev/comments/6q2tom/gb_changing_window_registers_midframe/
    // https://github.com/LIJI32/SameBoy/commit/cbbaf2ee843870d96e72380e2dedbdbda471ca76
    uint8_t wx_scx_compare = ((mmu->io_registers[IO_WX] & 7) == 7 - (mmu->io_registers[IO_SCX] & 7));
    if (ppu->saved_wly != -1 && !gb->is_cgb && !IS_WIN_ENABLED(gb) && ppu->is_wx_triggered && wx_scx_compare)
        return &glitched_pixel;

    if (ppu->pixel_fetcher.mode == FETCH_BG && IS_INSIDE_WINDOW(gb)) {
        // the fetcher must switch to window mode for the remaining of the scanline (except when an object needs to be drawn)
        ppu->pixel_fetcher.mode = FETCH_WIN;
        ppu->pixel_fetcher.old_mode = FETCH_WIN;
        ppu->pixel_fetcher.step = 0;
        ppu->pixel_fetcher.x = 0;

        if (ppu->saved_wly != -1) {
            // restore old wly if window was disabled and re-enabled now
            ppu->wly = ppu->saved_wly + 1; // + 1 because wly increments before every new LY where the window is drawn
            ppu->saved_wly = -1;
        } else if (ppu->wly == -1) {
            // if WY is written outside VBLANK and window was not triggered, wly becomes LY - WY
            //     --> it's as if the window was enabled since the beginning when LY was equal to WY
            // see example (this is what happens in pokemon gold/silver battle intro slide animation):
            //     LY == 106, read WY == 144              --> window not triggered
            //     LY == 106, write WY = 0 during VBLANK  --> window not triggered
            //     LY == 107, read WY == 0 during DRAWING --> window triggered (because WY <= LY) but it should have been triggered at LY == WY -> LY == 0
            //                                            --> to catch up we set wly to LY - WY: it's as if WY was 0 at the beginning of the frame
            //                                            --> BUT because we can't go backwards, the window above won't be visible
            ppu->wly = mmu->io_registers[IO_LY] - mmu->io_registers[IO_WY];
        } else {
            ppu->wly++; // wly increments before every new LY where the window is drawn
        }

        ppu->discarded_pixels = 0;
        pixel_fifo_clear(&ppu->bg_win_fifo);
    }

    return NULL;
}

/**
 * @returns 1 if a an OBJ fetch is in progress, 0 otherwise 
 */
static inline uint8_t handle_obj(gb_t *gb) {
    gb_ppu_t *ppu = gb->ppu;

    if (ppu->bg_win_fifo.size > 0 && ppu->pixel_fetcher.mode != FETCH_OBJ && ppu->oam_scan.index < ppu->oam_scan.size && ppu->oam_scan.objs[ppu->oam_scan.index].x <= ppu->lcd_x + 8) {
        if (IS_OBJ_ENABLED(gb)) {
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

    return ppu->pixel_fetcher.mode == FETCH_OBJ;
}

static inline void drawing_step(gb_t *gb) {
    gb_mmu_t *mmu = gb->mmu;
    gb_ppu_t *ppu = gb->ppu;

    if (!ppu->pixel_fetcher.is_dummy_fetch_done) {
        ppu->is_wx_triggered = mmu->io_registers[IO_WX] == ppu->pixel_fetcher.step;
    } else if (ppu->lcd_x == 0 && ppu->pixel_fetcher.step == 0) {
        // in this current cycle we just finished the dummy fetch
        // ppu->pixel_fetcher.step == 0 here but it should be equivalent to 6
        ppu->is_wx_triggered = mmu->io_registers[IO_WX] == 6;
    } else {
        ppu->is_wx_triggered = mmu->io_registers[IO_WX] - 7 == ppu->lcd_x;
    }

    // 1. FETCHING PIXELS

    // TODO I don't remember where I read this (might be wrong):
    // the fetcher is busy until its 5th out of 8 steps
    // --> an obj detected waits until the 5th step and can override the fetcher
    // --> an obj detected at the 5th step and above overrides immediatly the fetcher
    switch (ppu->pixel_fetcher.step++) {
    case GET_TILE_ID:
        fetch_tile_id(gb);
        break;
    case GET_TILE_SLICE_LOW:
        fetch_tileslice_low(gb);
        break;
    case GET_TILE_SLICE_HIGH:
        fetch_tileslice_high(gb);
        if (!ppu->pixel_fetcher.is_dummy_fetch_done) {
            ppu->pixel_fetcher.is_dummy_fetch_done = 1;
            ppu->pixel_fetcher.step = 0;
        }
        break;
    case PUSH:
        ppu->pixel_fetcher.step = push(gb) ? 0 : PUSH;
        break;
    }

    if (handle_obj(gb)) // pause pixel fetching while an obj fetch is in progress
        return;

    gb_pixel_t *glitched_pixel = handle_window(gb);

    // 2. SHIFTING PIXELS OUT TO THE LCD

    gb_pixel_t *bg_win_pixel = glitched_pixel ? glitched_pixel : pixel_fifo_pop(&ppu->bg_win_fifo);
    if (!bg_win_pixel)
        return;

    // discards pixels if needed, either due to SCX or window starting before screen (WX < 7)
    switch (ppu->pixel_fetcher.mode) {
    case FETCH_BG:
        if (ppu->discarded_pixels < mmu->io_registers[IO_SCX] % 8) {
            ppu->discarded_pixels++;
            return;
        }
        break;
    case FETCH_WIN:
        if (mmu->io_registers[IO_WX] < 7 && ppu->discarded_pixels < 7 - mmu->io_registers[IO_WX]) {
            ppu->discarded_pixels++;
            return;
        }
        break;
    default:
        break;
    }

    gb_pixel_t *obj_pixel = pixel_fifo_pop(&ppu->obj_fifo);
    gb_pixel_t *selected_pixel = select_pixel(gb, bg_win_pixel, obj_pixel);
    uint8_t selected_is_obj = selected_pixel == obj_pixel;

    if (gb->is_cgb) {
        if (IS_CGB_COMPAT_MODE(gb))
            selected_pixel->color = dmg_get_color(mmu, selected_pixel, selected_is_obj);

        uint8_t r, g, b;
        cgb_get_color(mmu, selected_pixel, selected_is_obj, !gb->disable_cgb_color_correction, &r, &g, &b);

        SET_PIXEL_CGB(gb, ppu->lcd_x, mmu->io_registers[IO_LY], r, g, b);
    } else {
        SET_PIXEL_DMG(gb, ppu->lcd_x, mmu->io_registers[IO_LY], dmg_get_color(mmu, selected_pixel, selected_is_obj));
    }

    ppu->lcd_x++;

    if (ppu->lcd_x >= GB_SCREEN_WIDTH) {
        // if (mmu->io_registers[IO_LY] == 0) {
        //     printf("%d in [172, 289]?\n", ppu->cycles - 80);
        // }
        // the new position is outside the screen, this scanline is done: go into HBLANK mode
        ppu->lcd_x = 0;

        reset_pixel_fetcher(ppu);

        pixel_fifo_clear(&ppu->bg_win_fifo);
        pixel_fifo_clear(&ppu->obj_fifo);

        ppu->oam_scan.index = 0;
        ppu->win_actually_enabled = 0;
        set_mode(gb, PPU_MODE_HBLANK);
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
    uint8_t obj_height = IS_OBJ_TALL(gb) ? 16 : 8;
    // NOTE: obj->x != 0 condition should not be checked even if ultimate gameboy talk says it should
    if (ppu->oam_scan.size < 10 && (obj->y <= mmu->io_registers[IO_LY] + 16) && (obj->y + obj_height > mmu->io_registers[IO_LY] + 16)) {
        int8_t i;
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
        ppu->win_actually_enabled = IS_WIN_ENABLED(gb);
        set_mode(gb, PPU_MODE_DRAWING);
    }
}

static inline void hblank_step(gb_t *gb) {
    gb_mmu_t *mmu = gb->mmu;
    gb_ppu_t *ppu = gb->ppu;

    if (ppu->cycles < SCANLINE_CYCLES)
        return;

    mmu->io_registers[IO_LY]++;
    ppu->cycles = 0;

    if (mmu->io_registers[IO_LY] == GB_SCREEN_HEIGHT) {
        ppu->wly = -1;
        ppu->saved_wly = -1;
        set_mode(gb, PPU_MODE_VBLANK);

        // skip first screen rendering after the LCD was just turned on
        if (ppu->is_lcd_turning_on) {
            ppu->is_lcd_turning_on = 0;
            return;
        }

        if (gb->on_new_frame)
            gb->on_new_frame(ppu->pixels);
    } else {
        ppu->oam_scan.size = 0;
        set_mode(gb, PPU_MODE_OAM);
        RESET_BIT(mmu->io_registers[IO_STAT], 2); // clear LY=LYC until ppu->cycles == 4
    }
}

static inline void vblank_step(gb_t *gb) {
    gb_mmu_t *mmu = gb->mmu;
    gb_ppu_t *ppu = gb->ppu;

    if (ppu->cycles == 4) {
        if (mmu->io_registers[IO_LY] == 153) {
            mmu->io_registers[IO_LY] = 0;
            UPDATE_STAT_LY_LYC_BIT(gb);
            ppu->is_last_vblank_line = 1;
        }
    } else if (ppu->cycles == 12) {
        if (ppu->is_last_vblank_line)
            ppu_update_stat_irq_line(gb);
    } else if (ppu->cycles == SCANLINE_CYCLES) {
        // Also check for LY == 0 even if it should be useless because a strange bug happens when using the tester binary
        // with gbmicrotest/is_if_set_during_ime0.gb: ppu->is_last_vblank_line can be 1 while LY is 144 which
        // causes the ppu to enter OAM too early and starting at LY == 144 instead of LY == 0 which causes
        // the LY to eventually go beyond its normal bounds and the ppu to draw outside its pixels buffer --> segfault
        // This bug only happens with the tester binary and is non reproducible inside GDB.
        if (ppu->is_last_vblank_line && mmu->io_registers[IO_LY] == 0) {
            // we actually are on line 153 but LY reads 0
            ppu->is_last_vblank_line = 0;
            ppu->oam_scan.size = 0;
            set_mode(gb, PPU_MODE_OAM);
        } else {
            mmu->io_registers[IO_LY]++; // increase on each new line
        }

        ppu->cycles = 0;
    }
}

void ppu_enable_lcd(gb_t *gb) {
    gb_ppu_t *ppu = gb->ppu;

    ppu->mode = PPU_MODE_OAM; // reading hblank mode but actually in OAM mode
    ppu->cycles = 8; // lcd is 8 cycles early when turning on
    ppu->is_lcd_turning_on = 1;
    UPDATE_STAT_LY_LYC_BIT(gb);
    ppu_update_stat_irq_line(gb);

    // advance oam_scan to take into account the 8 cycles early
    ppu->oam_scan.index = 3; // 4 objects scanned
    ppu->oam_scan.step = 1; // scan 5th object immediately
}

void ppu_disable_lcd(gb_t *gb) {
    gb_mmu_t *mmu = gb->mmu;
    gb_ppu_t *ppu = gb->ppu;

    PPU_SET_STAT_MODE(gb, PPU_MODE_HBLANK);
    mmu->io_registers[IO_LY] = 0;

    ppu->wly = -1;
    reset_pixel_fetcher(ppu);
    pixel_fifo_clear(&ppu->bg_win_fifo);
    pixel_fifo_clear(&ppu->obj_fifo);

    mmu->hdma.allow_hdma_block = mmu->hdma.type == HDMA && mmu->hdma.progress > 0;

    // white screen
    for (int x = 0; x < GB_SCREEN_WIDTH; x++) {
        for (int y = 0; y < GB_SCREEN_HEIGHT; y++) {
            if (gb->is_cgb) {
                uint8_t r, g, b;
                cgb_get_color(mmu, NULL, 0, !gb->disable_cgb_color_correction, &r, &g, &b);
                SET_PIXEL_CGB(gb, x, y, r, g, b);
            } else {
                SET_PIXEL_DMG(gb, x, y, DMG_WHITE);
            }
        }
    }

    if (gb->on_new_frame)
        gb->on_new_frame(gb->ppu->pixels);
}

void ppu_update_stat_irq_line(gb_t *gb) {
    if (!IS_LCD_ENABLED(gb))
        return;

    gb_ppu_t *ppu = gb->ppu;

    uint8_t old_stat_irq_line = ppu->stat_irq_line;

    ppu->stat_irq_line = IS_HBLANK_IRQ_STAT_ENABLED(gb) && ppu->mode == PPU_MODE_HBLANK;
    ppu->stat_irq_line |= IS_VBLANK_IRQ_STAT_ENABLED(gb) && ppu->mode == PPU_MODE_VBLANK;
    ppu->stat_irq_line |= IS_OAM_IRQ_STAT_ENABLED(gb) && ppu->mode == PPU_MODE_OAM;
    ppu->stat_irq_line |= IS_LY_LYC_IRQ_STAT_ENABLED(gb) && gb->mmu->io_registers[IO_LY] == gb->mmu->io_registers[IO_LYC];

    if (!old_stat_irq_line && ppu->stat_irq_line)
        CPU_REQUEST_INTERRUPT(gb, IRQ_STAT);
}

void ppu_step(gb_t *gb) {
    if (!IS_LCD_ENABLED(gb))
        return;

    gb_ppu_t *ppu = gb->ppu;

    // STAT ppu mode bits have 4 cycles of delay compared to the actual ppu mode (except for VBLANK)
    if (ppu->pending_stat_mode >= 0) {
        PPU_SET_STAT_MODE(gb, ppu->pending_stat_mode);
        ppu_update_stat_irq_line(gb);
        if (ppu->pending_stat_mode == PPU_MODE_VBLANK) // this condition only happens in CGB mode (see set_mode() function)
            CPU_REQUEST_INTERRUPT(gb, IRQ_VBLANK);
        ppu->pending_stat_mode = -1;
    }

    for (uint8_t cycles = 0; cycles < 4; cycles++) { // 4 cycles per step
        // check for LYC=LY at the 4th cycle
        if (ppu->cycles == 4) {
            UPDATE_STAT_LY_LYC_BIT(gb);
            ppu_update_stat_irq_line(gb);
        }

        switch (ppu->mode) {
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
    gb->ppu = xcalloc(1, sizeof(*gb->ppu));
    gb->ppu->wly = -1;
    gb->ppu->pending_stat_mode = -1;
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

#define SERIALIZED_FETCHER               \
    Y(pixel_fetcher.pixels, color)       \
    Y(pixel_fetcher.pixels, palette)     \
    Y(pixel_fetcher.pixels, attributes)  \
    Y(pixel_fetcher.pixels, oam_pos)     \
    X(pixel_fetcher.mode)                \
    X(pixel_fetcher.old_mode)            \
    X(pixel_fetcher.step)                \
    X(pixel_fetcher.curr_oam_index)      \
    X(pixel_fetcher.is_dummy_fetch_done) \
    X(pixel_fetcher.current_tile_id)     \
    X(pixel_fetcher.x)

#define SERIALIZED_MEMBERS       \
    X(mode)                      \
    X(pending_stat_mode)         \
    X(is_lcd_turning_on)         \
    X(cycles)                    \
    X(discarded_pixels)          \
    X(lcd_x)                     \
    X(is_wx_triggered)           \
    X(wly)                       \
    X(saved_wly)                 \
    X(win_actually_enabled)      \
    X(is_last_vblank_line)       \
    X(stat_irq_line)             \
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
