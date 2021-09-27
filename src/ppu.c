#include "ppu.h"
#include "types.h"
#include "mem.h"

// TODO classical green colors
// Grayscale colors
byte_t color_values[4][3] = {
    { 0xFF, 0xFF, 0xFF },
    { 0xCC, 0xCC, 0xCC },
    { 0x77, 0x77, 0x77 },
    { 0x00, 0x00, 0x00 }
};
// TODO? LCD off special bright white color
enum colors {
    WHITE,
    LIGH_GRAY,
    DARK_GRAY,
    BLACK
};
byte_t pixels[160 * 144 * 3];
#define SET_PIXEL(x, y, c) pixels[(y * 160 * 3) + (x * 3)] = color_values[c][0]; pixels[(y * 160 * 3) + (x * 3) + 1] = color_values[c][1]; pixels[(y * 160 * 3) + (x * 3) + 2] = color_values[c][2];

int ppu_cycles = 0;

// TODO easier to draw a scanline entirely at the same time when ppu_cycles >= time needed to draw a line
void ppu_step(int cycles) {
    ppu_cycles += cycles;

    SET_PIXEL(50, 50, WHITE);

    byte_t current_line = mem[LY];
    // printf("current_line=%2X\n", current_line);
    // TODO this is wrong...
    if (current_line >= 144) { // Mode 1 (VBlank)
        ppu_cycles = 0;

        if (current_line > 153) {
            mem[LY] = 0;
        } else {
            mem[LY]++; // we incement onto the next scanline --- when the game write to 0xFF44 it resets the scanline to 0 so no mem_write()
        }
    }

    if (ppu_cycles <= 80) { // Mode 2 (OAM)
        printf("OAM\n");
    } else if (ppu_cycles <= 252) { // Mode 3 (Drawing)
        printf("Drawing\n");
    } else if (ppu_cycles <= 456) { // Mode 0 (HBlank)
        printf("HBlank\n");
    }
}
