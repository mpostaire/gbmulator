#include "ppu.h"
#include "types.h"
#include "mem.h"

int ppu_cycles = 0;

// TODO easier to draw a scanline entirely at the same time when ppu_cycles >= time needed to draw a line
void ppu_step(int cycles) {
    ppu_cycles += cycles;

    byte_t current_line = mem[LY];
    printf("current_line=%2X\n", current_line);
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
