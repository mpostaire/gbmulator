#pragma once

#include "gba.h"
#include "cpu.h"
#include "bus.h"
#include "ppu.h"
#include "tmr.h"
#include "dma.h"

#include "../core_priv.h"

struct gba_t {
    const gbmulator_t *base;

    char rom_title[13]; // title is max 12 chars

    gba_cpu_t cpu;
    gba_bus_t bus;
    gba_ppu_t ppu;
    gba_dma_t dma;
    gba_tmr_t tmr;
};
