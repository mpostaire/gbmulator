#pragma once

#include "types.h"
#include "mmu.h"
#include "cpu.h"

#define PPU_IS_MODE(emu_ptr, m) (((emu_ptr)->mmu->mem[STAT] & 0x03) == (m))

typedef enum {
    PPU_MODE_HBLANK,
    PPU_MODE_VBLANK,
    PPU_MODE_OAM,
    PPU_MODE_DRAWING
} ppu_mode_t;

extern byte_t ppu_color_palettes[PPU_COLOR_PALETTE_MAX][4][3];

void inline ppu_ly_lyc_compare(emulator_t *emu) {
    mmu_t *mmu = emu->mmu;

    if (mmu->mem[LY] == mmu->mem[LYC]) {
        SET_BIT(mmu->mem[STAT], 2);
        if (CHECK_BIT(mmu->mem[STAT], 6))
            CPU_REQUEST_INTERRUPT(emu, IRQ_STAT);
    } else {
        RESET_BIT(mmu->mem[STAT], 2);
    }
}

void ppu_step(emulator_t *emu, int cycles);

void ppu_init(emulator_t *emu);

void ppu_quit(emulator_t *emu);
