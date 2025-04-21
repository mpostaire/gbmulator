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

#define SET_PIXEL(gba, x, y, r, g, b)                                            \
    do {                                                                         \
        (gba)->ppu->pixels[((y) * GBA_SCREEN_WIDTH * 4) + ((x) * 4)] = (r);      \
        (gba)->ppu->pixels[((y) * GBA_SCREEN_WIDTH * 4) + ((x) * 4) + 1] = (g);  \
        (gba)->ppu->pixels[((y) * GBA_SCREEN_WIDTH * 4) + ((x) * 4) + 2] = (b);  \
        (gba)->ppu->pixels[((y) * GBA_SCREEN_WIDTH * 4) + ((x) * 4) + 3] = 0xFF; \
    } while (0)

void gba_ppu_init(gba_t *gba) {
    gba->ppu = xcalloc(1, sizeof(*gba->ppu));
}

void gba_ppu_quit(gba_ppu_t *ppu) {
    free(ppu);
}

void gba_ppu_step(gba_t *gba) {
    gba_ppu_t *ppu = gba->ppu;

    if (ppu->pixel_cycles++ < PIXEL_CYCLES)
        return;

    ppu->pixel_cycles = 0;

    // TODO maybe do not compute period from ppu x/y pos but with cycles because ppu position is updated 4 cycles after a change of period

    switch (ppu->period) {
    case GBA_PPU_PERIOD_HDRAW:
        uint16_t color = gba_bus_read_half(gba, BUS_VRAM + (gba->bus->io_regs[IO_VCOUNT] * (GBA_SCREEN_WIDTH * 2) + (ppu->x * 2)));

        uint8_t r = color & 0x001F;
        uint8_t g = (color >> 5) & 0x001F;
        uint8_t b = (color >> 10) & 0x001F;

        r = (r << 3) | (r >> 2);
        g = (g << 3) | (g >> 2);
        b = (b << 3) | (b >> 2);

        SET_PIXEL(gba, ppu->x, gba->bus->io_regs[IO_VCOUNT], r, g, b);

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
                todo("VBLANK --> if no picture visible, check how to implement io registers");

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
