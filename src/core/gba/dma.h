#pragma once

#include "gba.h"

#define GBA_DMA_CHANNEL_COUNT 4

typedef struct {
    uint32_t src;
    uint32_t dst;
    uint32_t count;
    bool is_half;
    int8_t src_increment;
    int8_t dst_increment;
} gba_dma_channel_t;

typedef struct {
    uint8_t active_channels;
    gba_dma_channel_t channel[GBA_DMA_CHANNEL_COUNT];
} gba_dma_t;

void gba_dma_step(gba_t *gba);

void gba_dma_init(gba_t *gba);

void gba_dma_quit(gba_t *gba);
