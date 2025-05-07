#include "gba_priv.h"

#define VBLANK_HEIGHT 68

#define BG_FETCH_DELAY 31
#define COMPOSITING_DELAY 46

#define PIXEL_CYCLES 4
#define HDRAW_CYCLES 1006
#define HBLANK_CYCLES 226

#define SCANLINE_CYCLES (HDRAW_CYCLES + HBLANK_CYCLES)
#define VDRAW_CYCLES (SCANLINE_CYCLES * GBA_SCREEN_HEIGHT)
#define VBLANK_CYCLES (SCANLINE_CYCLES * VBLANK_HEIGHT)
#define REFRESH_CYCLES (VDRAW_CYCLES + VBLANK_CYCLES)

#define PPU_GET_MODE(gba) (gba)->bus->io_regs[IO_DISPCNT] & 0x07

#define PPU_GET_FRAME(gba) GET_BIT((gba)->bus->io_regs[IO_DISPCNT], 4)

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

    memset(gba->bus->palette_ram, 0xAA, sizeof(gba->bus->palette_ram));
}

void gba_ppu_quit(gba_ppu_t *ppu) {
    free(ppu);
}

static inline void draw_text_bg(gba_t *gba, uint8_t bg, uint32_t x, uint32_t y) {
    gba_ppu_t *ppu = gba->ppu;

    uint16_t bgxcnt = IO_BG0CNT + (bg * 2);
    uint16_t bgxvofs = IO_BG0VOFS + (bg * 2);
    uint16_t bgxhofs = IO_BG0HOFS + (bg * 2);

    uint8_t priority = gba->bus->io_regs[bgxcnt] & 0x03;
    uint8_t char_base_block = (gba->bus->io_regs[bgxcnt] >> 2) & 0x03;
    bool mosaic = CHECK_BIT(gba->bus->io_regs[bgxcnt], 6);
    bool is_8bpp = CHECK_BIT(gba->bus->io_regs[bgxcnt], 7);
    uint8_t screen_base_block = gba->bus->io_regs[bgxcnt + 1] & 0x1F;
    uint8_t screen_size = (gba->bus->io_regs[bgxcnt + 1] >> 6) & 0x03;

    uint16_t n_tiles_x = CHECK_BIT(screen_size, 0) ? 64 : 32;
    uint16_t n_tiles_y = CHECK_BIT(screen_size, 1) ? 64 : 32;

    uint32_t voffset = (gba->bus->io_regs[bgxvofs + 1] & 0x03) << 8 | gba->bus->io_regs[bgxvofs];
    uint32_t hoffset = (gba->bus->io_regs[bgxhofs + 1] & 0x03) << 8 | gba->bus->io_regs[bgxhofs];

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

    uint16_t tile_number = sbe & 0x03FF;
    bool flip_x = CHECK_BIT(sbe, 10);
    bool flip_y = CHECK_BIT(sbe, 11);

    uint32_t tile_x = base_x % 8;
    uint32_t tile_y = base_y % 8;

    if (flip_x)
        tile_x = 7 - tile_x;
    if (flip_y)
        tile_y = 7 - tile_y;

    if (is_8bpp) {
        uint32_t char_addr_offset = tile_number * 0x40;
        char_addr_offset += (tile_y * 8) + tile_x;

        uint32_t char_data_addr = char_base + char_addr_offset;

        if (char_data_addr >= BUS_VRAM + (4 * 0x4000)) {
            ppu->line_layers[bg][x] = 0;
            return;
        }

        ppu->line_layers[bg][x] = gba_bus_read_byte(gba, char_data_addr);
    } else { // 4bpp
        uint32_t char_addr_offset = tile_number * 0x20;
        char_addr_offset += (tile_y * 4) + tile_x / 2; // tile_y * 4 and tile_x / 2 because 4bpp

        uint32_t char_data_addr = char_base + char_addr_offset;

        if (char_data_addr >= BUS_VRAM + (4 * 0x4000)) {
            ppu->line_layers[bg][x] = 0;
            return;
        }

        uint8_t char_data = gba_bus_read_byte(gba, char_data_addr);

        if (x % 2)
            char_data >>= 4;
        else
            char_data &= 0x0F;

        uint8_t palette_index_hi = (sbe >> 12) & 0x0F;
        uint8_t palette_index_lo = char_data & 0x0F;

        ppu->line_layers[bg][x] = (palette_index_hi << 4) | palette_index_lo;
    }
}

static inline void draw_obj(gba_t *gba, uint32_t x, uint32_t y) {
    gba_ppu_t *ppu = gba->ppu;

    bool obj_char_vram_mapping = CHECK_BIT(gba->bus->io_regs[IO_DISPCNT], 6);

    uint8_t palette_index = 0;

    ppu->line_layers[4][x] = palette_index;
}

static inline void draw_mode0(gba_t *gba) {
    gba_ppu_t *ppu = gba->ppu;

    if (ppu->scanline_cycles < BG_FETCH_DELAY || (ppu->scanline_cycles % PIXEL_CYCLES) != PIXEL_CYCLES - 1)
        return;

    uint32_t x = (ppu->scanline_cycles - BG_FETCH_DELAY) / PIXEL_CYCLES;
    uint32_t y = gba->bus->io_regs[IO_VCOUNT];

    if (x >= GBA_SCREEN_WIDTH || y >= GBA_SCREEN_HEIGHT)
        return;

    bool bg0_enabled = CHECK_BIT(gba->bus->io_regs[IO_DISPCNT + 1], 0);
    if (bg0_enabled)
        draw_text_bg(gba, 0, x, y);

    bool bg1_enabled = CHECK_BIT(gba->bus->io_regs[IO_DISPCNT + 1], 1);
    if (bg1_enabled)
        draw_text_bg(gba, 1, x, y);

    bool bg2_enabled = CHECK_BIT(gba->bus->io_regs[IO_DISPCNT + 1], 2);
    if (bg2_enabled)
        draw_text_bg(gba, 2, x, y);

    bool bg3_enabled = CHECK_BIT(gba->bus->io_regs[IO_DISPCNT + 1], 3);
    if (bg3_enabled)
        draw_text_bg(gba, 3, x, y);

    bool obj_enabled = CHECK_BIT(gba->bus->io_regs[IO_DISPCNT + 1], 4);
    if (obj_enabled)
        draw_obj(gba, x, y);
}

static inline void draw_mode1(gba_t *gba) {
    // TODO
}

static inline void draw_mode2(gba_t *gba) {
    // TODO
}

static inline void draw_mode3(gba_t *gba) {
    gba_ppu_t *ppu = gba->ppu;

    if (ppu->scanline_cycles < BG_FETCH_DELAY || (ppu->scanline_cycles % PIXEL_CYCLES) != PIXEL_CYCLES - 1)
        return;

    uint32_t x = (ppu->scanline_cycles - BG_FETCH_DELAY) / PIXEL_CYCLES;
    uint32_t y = gba->bus->io_regs[IO_VCOUNT];

    if (x >= GBA_SCREEN_WIDTH || y >= GBA_SCREEN_HEIGHT)
        return;

    uint32_t pixel_addr_offset = (y << 1) * GBA_SCREEN_WIDTH + (x << 1);

    bool display_bg2 = CHECK_BIT(gba->bus->io_regs[IO_DISPCNT + 1], 2);
    if (display_bg2)
        ppu->line_layers[2][x] = gba_bus_read_half(gba, BUS_VRAM + pixel_addr_offset);
    else
        ppu->line_layers[2][x] = gba_bus_read_half(gba, BUS_PALETTE_RAM);
}

static inline void draw_mode4(gba_t *gba) {
    gba_ppu_t *ppu = gba->ppu;

    if (ppu->scanline_cycles < BG_FETCH_DELAY || (ppu->scanline_cycles % PIXEL_CYCLES) != PIXEL_CYCLES - 1)
        return;

    uint32_t x = (ppu->scanline_cycles - BG_FETCH_DELAY) / PIXEL_CYCLES;
    uint32_t y = gba->bus->io_regs[IO_VCOUNT];

    if (x >= GBA_SCREEN_WIDTH || y >= GBA_SCREEN_HEIGHT)
        return;

    bool display_bg2 = CHECK_BIT(gba->bus->io_regs[IO_DISPCNT + 1], 2);
    if (!display_bg2) {
        ppu->line_layers[2][x] = 0;
        return;
    }

    uint32_t pixel_base_addr = BUS_VRAM + (PPU_GET_FRAME(gba) * 0xA000);
    uint32_t pixel_addr_offset = y * GBA_SCREEN_WIDTH + x;

    ppu->line_layers[2][x] = gba_bus_read_byte(gba, pixel_base_addr + pixel_addr_offset);
}

static inline void draw_mode5(gba_t *gba) {
    gba_ppu_t *ppu = gba->ppu;

    if (ppu->scanline_cycles < BG_FETCH_DELAY || (ppu->scanline_cycles % PIXEL_CYCLES) != PIXEL_CYCLES - 1)
        return;

    uint32_t x = (ppu->scanline_cycles - BG_FETCH_DELAY) / PIXEL_CYCLES;
    uint32_t y = gba->bus->io_regs[IO_VCOUNT];

    if (x >= GBA_SCREEN_WIDTH || y >= GBA_SCREEN_HEIGHT)
        return;

    bool display_bg2 = CHECK_BIT(gba->bus->io_regs[IO_DISPCNT + 1], 2);
    if (!display_bg2 || x >= 160 || y >= 128) {
        ppu->line_layers[2][x] = gba_bus_read_half(gba, BUS_PALETTE_RAM);
        return;
    }

    uint32_t pixel_base_addr = BUS_VRAM + (PPU_GET_FRAME(gba) * 0xA000);
    uint32_t pixel_addr_offset = (y << 1) * 160 + (x << 1);

    ppu->line_layers[2][x] = gba_bus_read_half(gba, pixel_base_addr + pixel_addr_offset);
}

static inline void compositing(gba_t *gba) {
    // TODO priorities, alpha blending, greenswap, palette ram access timings, other bgs than bg2, objs

    gba_ppu_t *ppu = gba->ppu;

    if (ppu->scanline_cycles < COMPOSITING_DELAY || (ppu->scanline_cycles % PIXEL_CYCLES) != PIXEL_CYCLES - 1)
        return;

    uint32_t x = (ppu->scanline_cycles - COMPOSITING_DELAY) / PIXEL_CYCLES;
    uint32_t y = gba->bus->io_regs[IO_VCOUNT];

    if (x >= GBA_SCREEN_WIDTH || y >= GBA_SCREEN_HEIGHT)
        return;

    uint8_t mode = PPU_GET_MODE(gba);

    uint16_t color = gba_bus_read_half(gba, BUS_PALETTE_RAM); // backdrop color

    for (uint8_t i = 0; i < 5; i++) {
        bool bg_enabled = CHECK_BIT(gba->bus->io_regs[IO_DISPCNT + 1], i);
        if (!bg_enabled)
            continue;

        if (mode == 3 || mode == 5) {
            color = ppu->line_layers[i][x];
        } else {
            uint16_t palette_index = ppu->line_layers[i][x];
            if (palette_index != 0)
                color = gba_bus_read_half(gba, BUS_PALETTE_RAM + (palette_index << 1));
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
            draw_mode0(gba);
            break;
        case 1:
            draw_mode1(gba);
            break;
        case 2:
            draw_mode2(gba);
            break;
        case 3:
            draw_mode3(gba);
            break;
        case 4:
            draw_mode4(gba);
            break;
        case 5:
            draw_mode5(gba);
            break;
        default:
            todo();
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
