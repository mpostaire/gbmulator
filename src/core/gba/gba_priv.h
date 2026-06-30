#pragma once

#include <assert.h>

#include "gba.h"
#include "cpu.h"
#include "bus.h"
#include "ppu.h"
#include "tmr.h"
#include "dma.h"

#include "../core_priv.h"

typedef enum {
    GBA_SCHED_EVENT_PPU_ENTER_HDRAW,
    GBA_SCHED_EVENT_PPU_ENTER_VHBLANK, // same event for HBLANK in VDRAW and VBLANK periods

    GBA_SCHED_EVENT_TMR0_OVERFLOW, // non-cascading TMR0 event
    GBA_SCHED_EVENT_TMR1_OVERFLOW, // non-cascading TMR1 event
    GBA_SCHED_EVENT_TMR2_OVERFLOW, // non-cascading TMR2 event
    GBA_SCHED_EVENT_TMR3_OVERFLOW, // non-cascading TMR3 event

    GBA_SCHED_EVENT_END
} gba_sched_event_t;

static_assert(GBA_SCHED_EVENT_END <= SCHED_MAX_EVENTS);

struct gba_t {
    const gbmulator_t *base;

    char rom_title[13]; // title is max 12 chars

    sched_t sched;

    gba_cpu_t cpu;
    gba_bus_t bus;
    gba_ppu_t ppu;
    gba_dma_t dma;
    gba_tmr_t tmr;
};

static inline void gba_sched_add(gba_t *gba, gba_sched_event_t event, uint64_t delay, uint64_t period) {
    assert(event < GBA_SCHED_EVENT_END);
    sched_add(&gba->sched, event, gba, delay, period);
}

static inline void gba_sched_cancel(gba_t *gba, gba_sched_event_t event) {
    assert(event < GBA_SCHED_EVENT_END);
    sched_cancel(&gba->sched, event);
}
