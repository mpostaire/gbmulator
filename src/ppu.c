#include <SDL2/SDL.h>

#include "ppu.h"
#include "types.h"
#include "mem.h"
#include "cpu.h"

// Grayscale colors
byte_t color_gray[4][3] = {
    { 0xFF, 0xFF, 0xFF },
    { 0xCC, 0xCC, 0xCC },
    { 0x77, 0x77, 0x77 },
    { 0x00, 0x00, 0x00 }
};
// Original colors
byte_t color_green[4][3] = {
    { 0x9B, 0xBC, 0x0F },
    { 0x8B, 0xAC, 0x0F },
    { 0x30, 0x62, 0x30 },
    { 0x0F, 0x38, 0x0F }
};
#define COLOR_VALUES color_green
// TODO? LCD off special bright white color
enum colors {
    WHITE,
    LIGH_GRAY,
    DARK_GRAY,
    BLACK
};
byte_t pixels[160 * 144 * 3];
#define SET_PIXEL(x, y, c) pixels[(y * 160 * 3) + (x * 3)] = COLOR_VALUES[c][0]; pixels[(y * 160 * 3) + (x * 3) + 1] = COLOR_VALUES[c][1]; pixels[(y * 160 * 3) + (x * 3) + 2] = COLOR_VALUES[c][2];

int ppu_cycles = 0;

enum ppu_modes {
    PPU_HBLANK,
    PPU_VBLANK,
    PPU_OAM,
    PPU_DRAWING
};
#define IS_MODE(m) ((mem[STAT] & 0x03) == m)
#define SET_MODE(m) mem[STAT] = (mem[STAT] & ~0x03) | m

static void ppu_draw_scanline() {
    
}

/**
 * This does not implement the pixel FIFO but draws each scanline instantly when starting PPU_HBLANK (mode 0).
 * TODO if STAT interrupts are a problem, implement these corner cases: http://gameboy.mongenel.com/dmg/istat98.txt
 */
void ppu_step(int cycles, SDL_Renderer *renderer, SDL_Texture *texture) {
    ppu_cycles += cycles;

    if (!CHECK_BIT(mem[LCDC], 7)) { // is LCD disabled?
        // TODO not sure of the handling of LCD disabled
        // TODO LCD disabled should fill screen with a color brighter than WHITE
        ppu_cycles = 0;
        mem[LY] = 0;
        return;
    }

    if (mem[LY] == mem[LYC] && CHECK_BIT(mem[STAT], 6)) { // LY == LYC compare
        SET_BIT(mem[STAT], 2);
        cpu_request_interrupt(IRQ_STAT);
    } else {
        RESET_BIT(mem[STAT], 2);
    }

    if (mem[LY] == 144) { // Mode 1 (VBlank)
        if (!IS_MODE(PPU_VBLANK)) {
            SET_MODE(PPU_VBLANK);
            if (CHECK_BIT(mem[STAT], 4))
                cpu_request_interrupt(IRQ_STAT);
            cpu_request_interrupt(IRQ_VBLANK);
            
            // the frame is complete, it can be rendered
            SDL_UpdateTexture(texture, NULL, pixels, 160 * sizeof(byte_t) * 3);
            SDL_RenderCopy(renderer, texture, NULL, NULL);
            SDL_RenderPresent(renderer);
        }
    } else {
        if (ppu_cycles <= 80) { // Mode 2 (OAM) -- ends after 80 ppu_cycles
            if (!IS_MODE(PPU_OAM))
                SET_MODE(PPU_OAM);
                if (CHECK_BIT(mem[STAT], 5))
                    cpu_request_interrupt(IRQ_STAT);
        } else if (ppu_cycles <= 252) { // Mode 3 (Drawing) -- ends after 252 ppu_cycles
            if (!IS_MODE(PPU_DRAWING))
                SET_MODE(PPU_DRAWING);
        } else if (ppu_cycles <= 456) { // Mode 0 (HBlank) -- ends after 456 ppu_cycles
            if (!IS_MODE(PPU_HBLANK)) {
                SET_MODE(PPU_HBLANK);
                if (CHECK_BIT(mem[STAT], 3))
                    cpu_request_interrupt(IRQ_STAT);
                // draw scanline when we enter HBlank
                ppu_draw_scanline();
                printf("%d\n", mem[LY]);
            }
        }
    }

    if (ppu_cycles > 456) {
        ppu_cycles -= 456; // not reset to 0 because there may be leftover cycles we want to take into account for the next scanline
        if (++mem[LY] > 153)
            mem[LY] = 0;
    }
}
