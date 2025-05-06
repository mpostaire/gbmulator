#include "gba_priv.h"

#define HBLANK_WIDTH 68
#define VBLANK_HEIGHT 68

#define PIXEL_CYCLES 4
#define HDRAW_CYCLES (PIXEL_CYCLES * GBA_SCREEN_WIDTH)
#define HBLANK_CYCLES (PIXEL_CYCLES * HBLANK_WIDTH)

#define SCANLINE_CYCLES (HDRAW_CYCLES + HBLANK_CYCLES)
#define VDRAW_CYCLES (SCANLINE_CYCLES * GBA_SCREEN_HEIGHT)
#define VBLANK_CYCLES (SCANLINE_CYCLES * VBLANK_HEIGHT)
#define REFRESH_CYCLES (VDRAW_CYCLES + VBLANK_CYCLES)

#define PPU_GET_MODE(gba) (gba)->bus->io_regs[IO_DISPCNT] & 0x07

#define PPU_MODE0 0
#define PPU_MODE1 1
#define PPU_MODE2 2
#define PPU_MODE3 3
#define PPU_MODE4 4
#define PPU_MODE5 5

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

static inline uint16_t draw_text_bg(gba_t *gba, uint8_t bg, bool *is_transparent) {
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

    uint32_t base_x = ppu->x + hoffset;
    base_x %= n_tiles_x * 8;
    uint32_t base_y = gba->bus->io_regs[IO_VCOUNT] + voffset;
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

    uint32_t x = base_x % 8;
    uint32_t y = base_y % 8;

    if (flip_x)
        x = 7 - x;
    if (flip_y)
        y = 7 - y;

    if (is_8bpp) {
        uint32_t char_addr_offset = tile_number * 0x40;
        char_addr_offset += (y * 8) + x;

        uint32_t char_data_addr = char_base + char_addr_offset;

        if (char_data_addr >= BUS_VRAM + (4 * 0x4000)) {
            *is_transparent = true;
            return gba_bus_read_half(gba, BUS_PALETTE_RAM);
        }

        uint8_t char_data = gba_bus_read_byte(gba, char_data_addr);

        uint8_t palette_index = char_data;
        uint32_t palette_addr_offset = palette_index << 1;

        *is_transparent = palette_index == 0;

        return gba_bus_read_half(gba, BUS_PALETTE_RAM + palette_addr_offset);
    } else { // 4bpp
        uint32_t char_addr_offset = tile_number * 0x20;
        char_addr_offset += (y * 4) + x / 2; // y * 4 and x / 2 because 4bpp

        uint32_t char_data_addr = char_base + char_addr_offset;

        if (char_data_addr >= BUS_VRAM + (4 * 0x4000)) {
            *is_transparent = true;
            return gba_bus_read_half(gba, BUS_PALETTE_RAM);
        }

        uint8_t char_data = gba_bus_read_byte(gba, char_data_addr);

        if (x % 2)
            char_data >>= 4;
        else
            char_data &= 0x0F;

        uint8_t palette_index_hi = (sbe >> 12) & 0x0F;
        uint8_t palette_index_lo = char_data & 0x0F;

        uint8_t palette_index = (palette_index_hi << 4) | palette_index_lo;
        uint32_t palette_addr_offset = palette_index << 1;

        *is_transparent = palette_index == 0;

        return gba_bus_read_half(gba, BUS_PALETTE_RAM + palette_addr_offset);
    }
}

static inline uint16_t draw_obj(gba_t *gba, bool *is_transparent) {
    bool obj_char_vram_mapping = CHECK_BIT(gba->bus->io_regs[IO_DISPCNT], 6);

    uint8_t palette_index = 1;
    *is_transparent = palette_index == 0;

    return 0xFF;
}

static inline uint16_t draw_mode0(gba_t *gba) {
    bool bg0_enabled = CHECK_BIT(gba->bus->io_regs[IO_DISPCNT + 1], 0);
    bool bg1_enabled = CHECK_BIT(gba->bus->io_regs[IO_DISPCNT + 1], 1);
    bool bg2_enabled = CHECK_BIT(gba->bus->io_regs[IO_DISPCNT + 1], 2);
    bool bg3_enabled = CHECK_BIT(gba->bus->io_regs[IO_DISPCNT + 1], 3);
    bool obj_enabled = CHECK_BIT(gba->bus->io_regs[IO_DISPCNT + 1], 4);

    bool is_transparent;
    uint16_t color = gba_bus_read_half(gba, BUS_PALETTE_RAM);

    if (obj_enabled)
        color = draw_obj(gba, &is_transparent);

    // TODO bg priority
    if (is_transparent && bg0_enabled)
        color = draw_text_bg(gba, 0, &is_transparent);
    if (is_transparent && bg1_enabled)
        color = draw_text_bg(gba, 1, &is_transparent);
    if (is_transparent && bg2_enabled)
        color = draw_text_bg(gba, 2, &is_transparent);
    if (is_transparent && bg3_enabled)
        color = draw_text_bg(gba, 3, &is_transparent);

    return color;
}

static inline uint16_t draw_mode1(gba_t *gba) {
    return gba_bus_read_half(gba, BUS_PALETTE_RAM);
}

static inline uint16_t draw_mode2(gba_t *gba) {
    return gba_bus_read_half(gba, BUS_PALETTE_RAM);
}

static inline uint16_t draw_mode3(gba_t *gba) {
    gba_ppu_t *ppu = gba->ppu;

    bool display_bg2 = CHECK_BIT(gba->bus->io_regs[IO_DISPCNT + 1], 2);
    if (!display_bg2)
        return gba_bus_read_half(gba, BUS_PALETTE_RAM);

    uint32_t pixel_addr_offset = (gba->bus->io_regs[IO_VCOUNT] << 1) * GBA_SCREEN_WIDTH + (ppu->x << 1);

    return gba_bus_read_half(gba, BUS_VRAM + pixel_addr_offset);
}

static inline uint16_t draw_mode4(gba_t *gba) {
    gba_ppu_t *ppu = gba->ppu;

    bool display_bg2 = CHECK_BIT(gba->bus->io_regs[IO_DISPCNT + 1], 2);
    if (!display_bg2)
        return gba_bus_read_half(gba, BUS_PALETTE_RAM);

    uint32_t pixel_base_addr = BUS_VRAM + (PPU_GET_FRAME(gba) * 0xA000);
    uint32_t pixel_addr_offset = gba->bus->io_regs[IO_VCOUNT] * GBA_SCREEN_WIDTH + ppu->x;

    uint8_t palette_index = gba_bus_read_byte(gba, pixel_base_addr + pixel_addr_offset);
    uint32_t palette_addr_offset = palette_index << 1;

    return gba_bus_read_half(gba, BUS_PALETTE_RAM + palette_addr_offset);
}

static inline uint16_t draw_mode5(gba_t *gba) {
    gba_ppu_t *ppu = gba->ppu;

    bool display_bg2 = CHECK_BIT(gba->bus->io_regs[IO_DISPCNT + 1], 2);
    if (!display_bg2)
        return gba_bus_read_half(gba, BUS_PALETTE_RAM);

    if (gba->bus->io_regs[IO_VCOUNT] >= 128 || ppu->x >= 160)
        return gba_bus_read_half(gba, BUS_PALETTE_RAM);

    uint32_t pixel_base_addr = BUS_VRAM + (PPU_GET_FRAME(gba) * 0xA000);
    uint32_t pixel_addr_offset = (gba->bus->io_regs[IO_VCOUNT] << 1) * 160 + (ppu->x << 1);

    return gba_bus_read_half(gba, pixel_base_addr + pixel_addr_offset);
}

void gba_ppu_step(gba_t *gba) {
    gba_ppu_t *ppu = gba->ppu;

    if (ppu->pixel_cycles++ < PIXEL_CYCLES) // TODO this is only true in bitmap modes
        return;

    ppu->pixel_cycles = 0;

    // TODO maybe do not compute period from ppu x/y pos but with cycles because ppu position is updated 4 cycles after a change of period

    uint16_t color = 0;
    switch (ppu->period) {
    case GBA_PPU_PERIOD_HDRAW:
        switch (PPU_GET_MODE(gba)) {
        case PPU_MODE0:
            color = draw_mode0(gba);
            break;
        case PPU_MODE1:
            color = draw_mode1(gba);
            break;
        case PPU_MODE2:
            color = draw_mode2(gba);
            break;
        case PPU_MODE3:
            color = draw_mode3(gba);
            break;
        case PPU_MODE4:
            color = draw_mode4(gba);
            break;
        case PPU_MODE5:
            color = draw_mode5(gba);
            break;
        default:
            todo();
            break;
        }

        if (CHECK_BIT(gba->bus->io_regs[IO_GREENSWAP], 0))
            todo("green swap");

        SET_PIXEL_COLOR(gba, ppu->x, gba->bus->io_regs[IO_VCOUNT], color);

        ppu->x++;
        if (ppu->x >= GBA_SCREEN_WIDTH) {
            ppu->period = GBA_PPU_PERIOD_HBLANK;
            SET_BIT(gba->bus->io_regs[IO_DISPSTAT], 1);
        }
        break;
    case GBA_PPU_PERIOD_HBLANK:
        ppu->x++;
        if (ppu->x >= GBA_SCREEN_WIDTH + HBLANK_WIDTH) {
            ppu->x = 0;
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
        ppu->x++;
        if (ppu->x >= GBA_SCREEN_WIDTH + HBLANK_WIDTH) {
            ppu->x = 0;
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
