#pragma once

#include "gba.h"

#define GBA_TMR_COUNT 4

typedef struct {
    struct {
        uint64_t last_sync_cycle;
        uint16_t reload;
        uint16_t divider;
    } instance[GBA_TMR_COUNT];
} gba_tmr_t;

void gba_tmr_set(gba_t *gba, uint16_t data, uint8_t channel);

void gba_tmr0_overflow(gba_t *gba);

void gba_tmr1_overflow(gba_t *gba);

void gba_tmr2_overflow(gba_t *gba);

void gba_tmr3_overflow(gba_t *gba);

void gba_tmr_sync(gba_t *gba);

void gba_tmr_reset(gba_t *gba);
