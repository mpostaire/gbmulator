#pragma once

#include "types.h"
#include "mmu.h"
#include "cpu.h"

#define PPU_GET_MODE(emu_ptr) ((emu_ptr)->mmu->mem[STAT] & 0x03)
#define PPU_IS_MODE(emu_ptr, mode) (PPU_GET_MODE(emu_ptr) == (mode))

#define IS_LCD_ENABLED(emu_ptr) (CHECK_BIT((emu_ptr)->mmu->mem[LCDC], 7))

#define IS_LY_LYC_IRQ_STAT_ENABLED(emu_ptr) (CHECK_BIT((emu_ptr)->mmu->mem[STAT], 6))
#define IS_OAM_IRQ_STAT_ENABLED(emu_ptr) (CHECK_BIT((emu_ptr)->mmu->mem[STAT], 5))
#define IS_VBLANK_IRQ_STAT_ENABLED(emu_ptr) (CHECK_BIT((emu_ptr)->mmu->mem[STAT], 4))
#define IS_HBLANK_IRQ_STAT_ENABLED(emu_ptr) (CHECK_BIT((emu_ptr)->mmu->mem[STAT], 3))

typedef enum {
    PPU_MODE_HBLANK,
    PPU_MODE_VBLANK,
    PPU_MODE_OAM,
    PPU_MODE_DRAWING
} ppu_mode_t;

extern byte_t ppu_color_palettes[PPU_COLOR_PALETTE_MAX][4][3];

inline void ppu_ly_lyc_compare(emulator_t *emu) {
    mmu_t *mmu = emu->mmu;

    if (mmu->mem[LY] == mmu->mem[LYC]) {
        SET_BIT(mmu->mem[STAT], 2);
        if (IS_LY_LYC_IRQ_STAT_ENABLED(emu))
            CPU_REQUEST_INTERRUPT(emu, IRQ_STAT);
    } else {
        RESET_BIT(mmu->mem[STAT], 2);
    }
}

void ppu_step(emulator_t *emu);

void ppu_init(emulator_t *emu);

void ppu_quit(emulator_t *emu);

size_t ppu_serialized_length(emulator_t *emu);

byte_t *ppu_serialize(emulator_t *emu, size_t *size);

void ppu_unserialize(emulator_t *emu, byte_t *buf);
