#include "gba_priv.h"

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
    gba->ppu->cycles++;

    if (gba->ppu->cycles >= 280896) {
        static int f = 0;
        SET_PIXEL(gba, f % GBA_SCREEN_WIDTH, 0, 0xFF, 0, 0);
        f++;
        gba->ppu->cycles = 0;

        if (gba->on_new_frame)
            gba->on_new_frame(gba->ppu->pixels);
    }
}
