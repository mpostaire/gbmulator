#pragma once

#include "gba.h"

typedef struct {
    uint32_t regs[16];

    uint32_t banked_regs_8_12[2][5];
    uint32_t banked_regs_13_14[6][2];

    uint32_t cpsr; // current program status register
    uint32_t spsr[6];

    uint32_t pipeline[2]; // array of instructions (because it is a 3 stage pipeline, we just need to remember 2 instructions)
} gba_cpu_t;

void gba_cpu_step(gba_t *gba);

void gba_cpu_init(gba_t *gba);

void gba_cpu_quit(gba_cpu_t *cpu);
