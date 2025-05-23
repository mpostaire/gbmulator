#include "gba_priv.h"

#define VBLANK_HEIGHT 68

#define BG_FETCH_DELAY 31
#define OBJ_FETCH_DELAY 40
#define COMPOSITING_DELAY 46

#define PIXEL_CYCLES 4
#define HDRAW_CYCLES 1006
#define HBLANK_CYCLES 226

#define SCANLINE_CYCLES (HDRAW_CYCLES + HBLANK_CYCLES)
#define VDRAW_CYCLES (SCANLINE_CYCLES * GBA_SCREEN_HEIGHT)
#define VBLANK_CYCLES (SCANLINE_CYCLES * VBLANK_HEIGHT)
#define REFRESH_CYCLES (VDRAW_CYCLES + VBLANK_CYCLES)

#define PPU_GET_FRAME(gba) GET_BIT((gba)->bus->io_regs[IO_DISPCNT], 4)

#define VRAM_OBJ_BASE_ADDR (BUS_VRAM + (4 * 0x4000))

#define SET_PIXEL_RGB(gba, x, y, r, g, b)                                        \
    do {                                                                         \
        (gba)->ppu->pixels[((y) * GBA_SCREEN_WIDTH * 4) + ((x) * 4)] = (r);      \
        (gba)->ppu->pixels[((y) * GBA_SCREEN_WIDTH * 4) + ((x) * 4) + 1] = (g);  \
        (gba)->ppu->pixels[((y) * GBA_SCREEN_WIDTH * 4) + ((x) * 4) + 2] = (b);  \
        (gba)->ppu->pixels[((y) * GBA_SCREEN_WIDTH * 4) + ((x) * 4) + 3] = 0xFF; \
    } while (0)

#define SET_PIXEL_COLOR(gba, x, y, c)            \
    do {                                         \
        uint8_t r = c & 0x001F;                  \
        uint8_t g = (c >> 5) & 0x001F;           \
        uint8_t b = (c >> 10) & 0x001F;          \
        r = (r << 3) | (r >> 2);                 \
        g = (g << 3) | (g >> 2);                 \
        b = (b << 3) | (b >> 2);                 \
        SET_PIXEL_RGB((gba), (x), (y), r, g, b); \
    } while (0)

void gba_ppu_init(gba_t *gba) {
    gba->ppu = xcalloc(1, sizeof(*gba->ppu));
}

void gba_ppu_quit(gba_t *gba) {
    free(gba->ppu);
}

static inline uint8_t render_text_tile_8bpp(gba_t *gba, uint32_t tile_base_addr, uint16_t tile_id, uint32_t x, uint32_t y, bool flip_x, bool flip_y) {
    uint32_t tile_x = x % 8;
    uint32_t tile_y = y % 8;

    if (flip_x)
        tile_x = 7 - tile_x;
    if (flip_y)
        tile_y = 7 - tile_y;

    uint32_t char_addr_offset = tile_id * 0x40;
    char_addr_offset += (tile_y * 8) + tile_x;

    uint32_t char_data_addr = tile_base_addr + char_addr_offset;

    if (tile_base_addr < VRAM_OBJ_BASE_ADDR && char_data_addr >= VRAM_OBJ_BASE_ADDR)
        return 0;

    return gba_bus_read_byte(gba, char_data_addr);
}

static inline uint8_t render_text_tile_4bpp(gba_t *gba, uint32_t tile_base_addr, uint16_t tile_id, uint32_t x, uint32_t y, bool flip_x, bool flip_y) {
    uint32_t tile_x = x % 8;
    uint32_t tile_y = y % 8;

    if (flip_x)
        tile_x = 7 - tile_x;
    if (flip_y)
        tile_y = 7 - tile_y;

    uint32_t char_addr_offset = tile_id * 0x20;
    char_addr_offset += (tile_y * 4) + tile_x / 2; // tile_y * 4 and tile_x / 2 because 4bpp

    uint32_t char_data_addr = tile_base_addr + char_addr_offset;

    if (tile_base_addr < VRAM_OBJ_BASE_ADDR && char_data_addr >= VRAM_OBJ_BASE_ADDR)
        return 0;

    uint8_t char_data = gba_bus_read_byte(gba, char_data_addr);

    if (tile_x % 2)
        char_data >>= 4;
    else
        char_data &= 0x0F;

    uint8_t palette_index_lo = char_data & 0x0F;

    return palette_index_lo;
}

static inline void draw_text_bg(gba_t *gba, uint8_t bg, uint32_t x, uint32_t y) {
    gba_ppu_t *ppu = gba->ppu;

    uint16_t bgxcnt = IO_BG0CNT + bg;
    uint16_t bgxvofs = IO_BG0VOFS + bg;
    uint16_t bgxhofs = IO_BG0HOFS + bg;

    uint8_t priority = gba->bus->io_regs[bgxcnt] & 0x03;
    uint8_t char_base_block = (gba->bus->io_regs[bgxcnt] >> 2) & 0x03;
    bool mosaic = CHECK_BIT(gba->bus->io_regs[bgxcnt], 6);
    bool is_8bpp = CHECK_BIT(gba->bus->io_regs[bgxcnt], 7);
    uint8_t screen_base_block = (gba->bus->io_regs[bgxcnt] >> 8) & 0x1F;
    uint8_t screen_size = (gba->bus->io_regs[bgxcnt] >> 14) & 0x03;

    uint16_t n_tiles_x = CHECK_BIT(screen_size, 0) ? 64 : 32;
    uint16_t n_tiles_y = CHECK_BIT(screen_size, 1) ? 64 : 32;

    uint32_t voffset = gba->bus->io_regs[bgxvofs] & 0x03FF;
    uint32_t hoffset = gba->bus->io_regs[bgxhofs] & 0x03FF;

    uint32_t base_x = x + hoffset;
    base_x %= n_tiles_x * 8;
    uint32_t base_y = y + voffset;
    base_y %= n_tiles_y * 8;

    uint32_t screen_block = 0;
    switch (screen_size) {
    case 0b00:
        break;
    case 0b01:
        if (base_x >= 256) {
            screen_block = 1;
            base_x %= 256;
        }
        break;
    case 0b10:
        if (base_y >= 256) {
            screen_block = 1;
            base_y %= 256;
        }
        break;
    case 0b11:
        if (base_x >= 256 && base_y >= 256) {
            screen_block = 3;
            base_x %= 256;
            base_y %= 256;
        } else if (base_x >= 256) {
            screen_block = 1;
            base_x %= 256;
        } else if (base_y >= 256) {
            screen_block = 2;
            base_y %= 256;
        }
        break;
    default:
        todo("this should never happen");
        break;
    }

    uint32_t sbe_base = BUS_VRAM + ((screen_base_block + screen_block) * 0x0800);
    uint32_t char_base = BUS_VRAM + (char_base_block * 0x4000);


    uint32_t sbe_addr_offset = (base_y / 8) * 32 + (base_x / 8); // y * 32 because a screenblock can fit 32x32 sbe
    sbe_addr_offset *= 2;
    uint16_t sbe = gba_bus_read_half(gba, sbe_base + sbe_addr_offset);

    uint16_t tile_id = sbe & 0x03FF;
    bool flip_x = CHECK_BIT(sbe, 10);
    bool flip_y = CHECK_BIT(sbe, 11);

    if (is_8bpp) {
        ppu->line_layers[bg][x] = render_text_tile_8bpp(gba, char_base, tile_id, base_x, base_y, flip_x, flip_y);
    } else { // 4bpp
        uint8_t palette_bank = (sbe >> 12) & 0x0F;
        // store palette bank in hi byte of line layer to be used by compositing step later
        ppu->line_layers[bg][x] = (palette_bank << 8) | render_text_tile_4bpp(gba, char_base, tile_id, base_x, base_y, flip_x, flip_y);
    }
}

static inline void draw_affine_bg(gba_t *gba, uint8_t bg, uint32_t x, uint32_t y) {
    // TODO
}

static inline void draw_obj(gba_t *gba) {
    gba_ppu_t *ppu = gba->ppu;

    if (ppu->scanline_cycles < OBJ_FETCH_DELAY || (ppu->scanline_cycles % PIXEL_CYCLES) != PIXEL_CYCLES - 1)
        return;

    int32_t x = (ppu->scanline_cycles - OBJ_FETCH_DELAY) / PIXEL_CYCLES;
    int32_t y = gba->bus->io_regs[IO_VCOUNT];

    if (x >= GBA_SCREEN_WIDTH)
        return;

    bool is_1d_mapping = CHECK_BIT(gba->bus->io_regs[IO_DISPCNT], 6);

    uint32_t obj_id = 0;

    uint32_t attrs01 = gba_bus_read_word(gba, BUS_OAM + (obj_id * 6));

    int32_t obj_y = attrs01 & 0xFF;
    if (obj_y >= GBA_SCREEN_HEIGHT)
        obj_y -= 256;
    uint8_t om = (attrs01 >> 8) & 0x03;
    uint8_t gm = (attrs01 >> 10) & 0x03;
    bool mos = CHECK_BIT(attrs01, 12);
    bool is_8bpp = CHECK_BIT(attrs01, 13);
    uint8_t sh = (attrs01 >> 14) & 0x03;

    int32_t obj_x = (attrs01 >> 16) & 0x01FF;
    if (obj_x >= GBA_SCREEN_WIDTH)
        obj_x -= 512;
    bool flip_x = CHECK_BIT(attrs01, 28);
    bool flip_y = CHECK_BIT(attrs01, 29);
    uint8_t sz = (attrs01 >> 30) & 0x03;

    static const uint8_t obj_dims[3][4][2] = {
        {{8, 8}, {16, 16}, {32, 32}, {64, 64}},
        {{16, 8}, {32, 8}, {32, 16}, {64, 32}},
        {{8, 16}, {8, 32}, {16, 32}, {32, 64}}
    };

    if (sh == 0b11)
        todo("forbidden");

    uint8_t obj_w = obj_dims[sh][sz][0];
    uint8_t obj_h = obj_dims[sh][sz][1];

    bool ignore_pixel = om == 0b10 || y < obj_y || y >= obj_y + obj_h || x < obj_x || x >= obj_x + obj_w;
    if (ignore_pixel) {
        ppu->line_layers[4][x] = 0;
        return;
    }

    uint16_t attr2 = gba_bus_read_half(gba, BUS_OAM + (obj_id * 6) + 4);

    uint16_t base_tile_id = attr2 & 0x03FF;
    uint8_t priority = (attr2 >> 10) & 0x03;

    uint16_t mapping_width = is_1d_mapping ? 8 : 32;

    uint32_t tile_x = x - obj_x;
    uint32_t tile_y = y - obj_y;

    if (flip_x)
        tile_x = (obj_w - 1) - tile_x;
    if (flip_y)
        tile_y = (obj_h - 1) - tile_y;

    uint16_t tile_id = base_tile_id + ((tile_y / 8) * mapping_width) + tile_x / 8;

    if (is_8bpp) {
        ppu->line_layers[4][x] = render_text_tile_8bpp(gba, VRAM_OBJ_BASE_ADDR, tile_id, x - obj_x, y - obj_y, flip_x, flip_y);
    } else { // 4bpp
        uint8_t palette_bank = (attr2 >> 12) & 0x0F;
        // store palette bank in hi byte of line layer to be used by compositing step later
        ppu->line_layers[4][x] = (palette_bank << 8) | render_text_tile_4bpp(gba, VRAM_OBJ_BASE_ADDR, tile_id, x - obj_x, y - obj_y, flip_x, flip_y);
    }
}

static inline void draw_bg_mode0(gba_t *gba) {
    gba_ppu_t *ppu = gba->ppu;

    if (ppu->scanline_cycles < BG_FETCH_DELAY || (ppu->scanline_cycles % PIXEL_CYCLES) != PIXEL_CYCLES - 1)
        return;

    uint32_t x = (ppu->scanline_cycles - BG_FETCH_DELAY) / PIXEL_CYCLES;
    uint32_t y = gba->bus->io_regs[IO_VCOUNT];

    if (x >= GBA_SCREEN_WIDTH)
        return;

    for (uint8_t bg = 0; bg < 4; bg++)
        if (CHECK_BIT(gba->bus->io_regs[IO_DISPCNT], bg + 8))
            draw_text_bg(gba, bg, x, y);
}

static inline void draw_bg_mode1(gba_t *gba) {
    gba_ppu_t *ppu = gba->ppu;

    if (ppu->scanline_cycles < BG_FETCH_DELAY || (ppu->scanline_cycles % PIXEL_CYCLES) != PIXEL_CYCLES - 1)
        return;

    uint32_t x = (ppu->scanline_cycles - BG_FETCH_DELAY) / PIXEL_CYCLES;
    uint32_t y = gba->bus->io_regs[IO_VCOUNT];

    if (x >= GBA_SCREEN_WIDTH)
        return;

    for (uint8_t bg = 0; bg < 2; bg++)
        if (CHECK_BIT(gba->bus->io_regs[IO_DISPCNT], bg + 8))
            draw_text_bg(gba, bg, x, y);

    if (CHECK_BIT(gba->bus->io_regs[IO_DISPCNT], 10))
        draw_affine_bg(gba, 2, x, y);
}

static inline void draw_bg_mode2(gba_t *gba) {
    gba_ppu_t *ppu = gba->ppu;

    if (ppu->scanline_cycles < BG_FETCH_DELAY || (ppu->scanline_cycles % PIXEL_CYCLES) != PIXEL_CYCLES - 1)
        return;

    uint32_t x = (ppu->scanline_cycles - BG_FETCH_DELAY) / PIXEL_CYCLES;
    uint32_t y = gba->bus->io_regs[IO_VCOUNT];

    if (x >= GBA_SCREEN_WIDTH)
        return;

    for (uint8_t bg = 2; bg < 4; bg++)
        if (CHECK_BIT(gba->bus->io_regs[IO_DISPCNT], bg + 8))
            draw_affine_bg(gba, bg, x, y);
}

static inline void draw_bg_mode3(gba_t *gba) {
    gba_ppu_t *ppu = gba->ppu;

    if (ppu->scanline_cycles < BG_FETCH_DELAY || (ppu->scanline_cycles % PIXEL_CYCLES) != PIXEL_CYCLES - 1)
        return;

    uint32_t x = (ppu->scanline_cycles - BG_FETCH_DELAY) / PIXEL_CYCLES;
    uint32_t y = gba->bus->io_regs[IO_VCOUNT];

    if (x >= GBA_SCREEN_WIDTH)
        return;

    bool display_bg2 = CHECK_BIT(gba->bus->io_regs[IO_DISPCNT], 10);
    if (display_bg2) {
        uint32_t pixel_base_addr = BUS_VRAM;
        uint32_t pixel_addr_offset = (y << 1) * GBA_SCREEN_WIDTH + (x << 1);

        ppu->line_layers[2][x] = gba_bus_read_half(gba, pixel_base_addr + pixel_addr_offset);
    } else {
        ppu->line_layers[2][x] = gba_bus_read_half(gba, BUS_PRAM);
    }
}

static inline void draw_bg_mode4(gba_t *gba) {
    gba_ppu_t *ppu = gba->ppu;

    if (ppu->scanline_cycles < BG_FETCH_DELAY || (ppu->scanline_cycles % PIXEL_CYCLES) != PIXEL_CYCLES - 1)
        return;

    uint32_t x = (ppu->scanline_cycles - BG_FETCH_DELAY) / PIXEL_CYCLES;
    uint32_t y = gba->bus->io_regs[IO_VCOUNT];

    if (x >= GBA_SCREEN_WIDTH)
        return;

    bool display_bg2 = CHECK_BIT(gba->bus->io_regs[IO_DISPCNT], 10);
    if (display_bg2) {
        uint32_t pixel_base_addr = BUS_VRAM + (PPU_GET_FRAME(gba) * 0xA000);
        uint32_t pixel_addr_offset = y * GBA_SCREEN_WIDTH + x;

        ppu->line_layers[2][x] = gba_bus_read_byte(gba, pixel_base_addr + pixel_addr_offset);
    } else {
        ppu->line_layers[2][x] = 0;
    }
}

static inline void draw_bg_mode5(gba_t *gba) {
    gba_ppu_t *ppu = gba->ppu;

    if (ppu->scanline_cycles < BG_FETCH_DELAY || (ppu->scanline_cycles % PIXEL_CYCLES) != PIXEL_CYCLES - 1)
        return;

    uint32_t x = (ppu->scanline_cycles - BG_FETCH_DELAY) / PIXEL_CYCLES;
    uint32_t y = gba->bus->io_regs[IO_VCOUNT];

    if (x >= GBA_SCREEN_WIDTH)
        return;

    bool display_bg2 = CHECK_BIT(gba->bus->io_regs[IO_DISPCNT], 10);
    if (display_bg2 || x >= 160 || y >= 128) {
        uint32_t pixel_base_addr = BUS_VRAM + (PPU_GET_FRAME(gba) * 0xA000);
        uint32_t pixel_addr_offset = (y << 1) * 160 + (x << 1);
    
        ppu->line_layers[2][x] = gba_bus_read_half(gba, pixel_base_addr + pixel_addr_offset);
    } else {
        ppu->line_layers[2][x] = gba_bus_read_half(gba, BUS_PRAM);
    }
}

static inline void compositing(gba_t *gba) {
    // TODO priorities, alpha blending, greenswap, palette ram access timings, other bgs than bg2, objs

    gba_ppu_t *ppu = gba->ppu;

    if (ppu->scanline_cycles < COMPOSITING_DELAY || (ppu->scanline_cycles % PIXEL_CYCLES) != PIXEL_CYCLES - 1)
        return;

    uint32_t x = (ppu->scanline_cycles - COMPOSITING_DELAY) / PIXEL_CYCLES;
    uint32_t y = gba->bus->io_regs[IO_VCOUNT];

    if (x >= GBA_SCREEN_WIDTH)
        return;

    uint8_t mode = PPU_GET_MODE(gba);

    uint16_t color = gba_bus_read_half(gba, BUS_PRAM); // backdrop color

    for (uint8_t i = 0; i < 5; i++) {
        bool bg_enabled = CHECK_BIT(gba->bus->io_regs[IO_DISPCNT], i + 8);
        if (!bg_enabled)
            continue;

        if (mode == 3 || mode == 5) {
            color = ppu->line_layers[i][x];
        } else {
            uint16_t palette_bank = ppu->line_layers[i][x] >> 8;
            uint16_t palette_index = ppu->line_layers[i][x] & 0x0F;

            if (palette_index != 0) {
                if (palette_bank)
                    palette_index |= palette_bank << 4;
                uint32_t palette_base_addr = i == 4 ? BUS_PRAM + 0x0200 : BUS_PRAM;
                color = gba_bus_read_half(gba, palette_base_addr + (palette_index << 1));
            }
        }
    }

    SET_PIXEL_COLOR(gba, x, y, color);
}

void gba_ppu_step(gba_t *gba) {
    gba_ppu_t *ppu = gba->ppu;

    ppu->scanline_cycles++; // TODO at the start or end of this func?

    // TODO bg state machine that emulate accurate bg vram accesses
    // TODO obj state machine that emulate accurate obj vram accesses --> renders objs 1 line before the current line

    switch (ppu->period) {
    case GBA_PPU_PERIOD_HDRAW:
        switch (PPU_GET_MODE(gba)) {
        case 0:
            draw_bg_mode0(gba);

            if (CHECK_BIT(gba->bus->io_regs[IO_DISPCNT], 12))
                draw_obj(gba);
            break;
        case 1:
            draw_bg_mode1(gba);
            break;
        case 2:
            draw_bg_mode2(gba);

            if (CHECK_BIT(gba->bus->io_regs[IO_DISPCNT], 12))
                draw_obj(gba);
            break;
        case 3:
            draw_bg_mode3(gba);
            break;
        case 4:
            draw_bg_mode4(gba);
            break;
        case 5:
            draw_bg_mode5(gba);
            break;
        default:
            break;
        }

        if (CHECK_BIT(gba->bus->io_regs[IO_GREENSWAP], 0))
            todo("green swap");

        // TODO composite step
        compositing(gba);

        // TODO  Although the drawing time is only 960 cycles (240*4), the H-Blank flag is "0" for a total of 1006 cycles.
        // --> 1006 - 960 == 46 --> this 46 offset is the composite offset?
        // so we enter hblank really at 1006 cycles not 960

        if (ppu->scanline_cycles >= HDRAW_CYCLES) {
            ppu->period = GBA_PPU_PERIOD_HBLANK;
            SET_BIT(gba->bus->io_regs[IO_DISPSTAT], 1);
        }
        break;
    case GBA_PPU_PERIOD_HBLANK:
        if (ppu->scanline_cycles >= SCANLINE_CYCLES) {
            ppu->scanline_cycles = 0;

            gba->bus->io_regs[IO_VCOUNT]++;

            ppu->period = GBA_PPU_PERIOD_HDRAW;
            if (gba->bus->io_regs[IO_VCOUNT] >= GBA_SCREEN_HEIGHT) {
                ppu->period = GBA_PPU_PERIOD_VBLANK;
                RESET_BIT(gba->bus->io_regs[IO_DISPSTAT], 1);
                SET_BIT(gba->bus->io_regs[IO_DISPSTAT], 0);
            }
        }
        break;
    case GBA_PPU_PERIOD_VBLANK:
        if (ppu->scanline_cycles >= SCANLINE_CYCLES) {
            ppu->scanline_cycles = 0;

            gba->bus->io_regs[IO_VCOUNT]++;

            if (gba->bus->io_regs[IO_VCOUNT] >= GBA_SCREEN_HEIGHT + VBLANK_HEIGHT) {
                gba->bus->io_regs[IO_VCOUNT] = 0;
                ppu->period = GBA_PPU_PERIOD_HDRAW;
                RESET_BIT(gba->bus->io_regs[IO_DISPSTAT], 0);

                if (gba->on_new_frame)
                    gba->on_new_frame(ppu->pixels);
            }
        }
        break;
    }
}
