#pragma once

#include "gb.h"
#include "cpu.h"
#include "mmu.h"
#include "dma.h"
#include "ppu.h"
#include "apu.h"
#include "timer.h"
#include "joypad.h"
#include "link.h"
#include "camera.h"

#include "../core_priv.h"

struct gb_t {
    const gbmulator_t *base;

    // this is true if CGB is in CGB mode, false if it is in DMG compatibility mode
    // TODO understand this better because what's the difference with gb->base->opts.mode == GBMULATOR_MODE_GBC?
    bool cgb_mode_enabled;

    uint8_t dmg_palette;

    char rom_title[17];

    gb_cpu_t    cpu;
    gb_mmu_t    mmu;
    gb_ppu_t    ppu;
    apu_t       apu;
    gb_timer_t  timer;
    gb_joypad_t joypad;
    gb_link_t   link;
};
