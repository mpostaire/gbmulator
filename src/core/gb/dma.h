#pragma once

#include "types.h"
#include "mmu.h"

typedef enum {
    GDMA,
    HDMA
} gb_hdma_type_t;

typedef enum {
    OAM_DMA_NO_INIT,
    OAM_DMA_INIT_BEGIN,
    OAM_DMA_INIT_PENDING,
    OAM_DMA_STARTING
} gb_oam_dma_starting_state_t;

#define IS_OAM_DMA_RUNNING(mmu) ((mmu)->oam_dma.progress >= 0 && (mmu)->oam_dma.progress < 0xA0)
#define GBC_GDMA_HDMA_LENGTH(mmu) ((mmu)->io_registers[IO_HDMA5] & 0x7F)

void dma_step(gb_t *gb);
