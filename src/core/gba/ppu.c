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

static inline void draw_mode3(gba_t *gba) {
    gba_ppu_t *ppu = gba->ppu;

    uint32_t pixel_addr_offset = (gba->bus->io_regs[IO_VCOUNT] << 1) * GBA_SCREEN_WIDTH + (ppu->x << 1);

    uint16_t color = gba_bus_read_half(gba, BUS_VRAM + pixel_addr_offset);

    SET_PIXEL_COLOR(gba, ppu->x, gba->bus->io_regs[IO_VCOUNT], color);
}

static inline void draw_mode4(gba_t *gba) {
    gba_ppu_t *ppu = gba->ppu;

    uint32_t pixel_base_addr = BUS_VRAM + (PPU_GET_FRAME(gba) * 0xA000);
    uint32_t pixel_addr_offset = gba->bus->io_regs[IO_VCOUNT] * GBA_SCREEN_WIDTH + ppu->x;

    uint8_t palette_index = gba_bus_read_byte(gba, pixel_base_addr + pixel_addr_offset);
    uint32_t palette_addr_offset = palette_index << 1;

    uint16_t color = gba_bus_read_half(gba, BUS_PALETTE_RAM + palette_addr_offset);

    SET_PIXEL_COLOR(gba, ppu->x, gba->bus->io_regs[IO_VCOUNT], color);
}

static inline void draw_mode5(gba_t *gba) {
    gba_ppu_t *ppu = gba->ppu;

    if (gba->bus->io_regs[IO_VCOUNT] >= 128 || ppu->x >= 160) {
        SET_PIXEL_COLOR(gba, ppu->x, gba->bus->io_regs[IO_VCOUNT], 0xFFFF); // TODO what color in this case?
        return;
    }

    uint32_t pixel_base_addr = BUS_VRAM + (PPU_GET_FRAME(gba) * 0xA000);
    uint32_t pixel_addr_offset = (gba->bus->io_regs[IO_VCOUNT] << 1) * 160 + (ppu->x << 1);

    uint16_t color = gba_bus_read_half(gba, pixel_base_addr + pixel_addr_offset);

    SET_PIXEL_COLOR(gba, ppu->x, gba->bus->io_regs[IO_VCOUNT], color);
}

void gba_ppu_step(gba_t *gba) {
    gba_ppu_t *ppu = gba->ppu;

    if (ppu->pixel_cycles++ < PIXEL_CYCLES)
        return;

    ppu->pixel_cycles = 0;

    // TODO maybe do not compute period from ppu x/y pos but with cycles because ppu position is updated 4 cycles after a change of period

    switch (ppu->period) {
    case GBA_PPU_PERIOD_HDRAW:
        switch (PPU_GET_MODE(gba)) {
        case PPU_MODE0:
            break;
        case PPU_MODE1:
            break;
        case PPU_MODE2:
            break;
        case PPU_MODE3:
            draw_mode3(gba);
            break;
        case PPU_MODE4:
            draw_mode4(gba);
            break;
        case PPU_MODE5:
            draw_mode5(gba);
            break;
        default:
            todo();
            break;
        }

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
