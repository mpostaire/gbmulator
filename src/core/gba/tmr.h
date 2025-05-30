#pragma once

#include "gba.h"

#define GBA_TMR_COUNT 4

typedef struct {
    struct {
        uint32_t cycle;
        uint16_t reload;
        uint16_t prescaler;
    } instance[GBA_TMR_COUNT];
} gba_tmr_t;

void gba_tmr_set(gba_t *gba, uint16_t data, uint8_t channel);

void gba_tmr_step(gba_t *gba);

void gba_tmr_init(gba_t *gba);

void gba_tmr_quit(gba_t *gba);
