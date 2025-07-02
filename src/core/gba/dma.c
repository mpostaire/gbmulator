#include <stdbit.h>

#include "gba_priv.h"

#define IO_DMAxCNT_H(gba, channel) (gba)->bus->io_regs[(IO_DMA0CNT_H + ((IO_DMA1CNT_H - IO_DMA0CNT_H) * (channel)))]
#define IO_DMAxCNT_L(gba, channel) (gba)->bus->io_regs[(IO_DMA0CNT_L + ((IO_DMA1CNT_L - IO_DMA0CNT_L) * (channel)))]
#define IO_DMAxCNT(gba, channel) ((IO_DMAxCNT_H(gba, channel) << 8) | IO_DMAxCNT_L(gba, channel))

#define _IO_DMAxSAD(gba, channel, half) (gba)->bus->io_regs[(IO_DMA0SAD + ((IO_DMA1SAD - IO_DMA0SAD) * (channel))) + (half)]
#define IO_DMAxSAD(gba, channel) ((_IO_DMAxSAD(gba, channel, 1) << 16) | _IO_DMAxSAD(gba, channel, 0))

#define _IO_DMAxDAD(gba, channel, half) (gba)->bus->io_regs[(IO_DMA0DAD + ((IO_DMA1DAD - IO_DMA0DAD) * (channel))) + (half)]
#define IO_DMAxDAD(gba, channel) ((_IO_DMAxDAD(gba, channel, 1) << 16) | _IO_DMAxDAD(gba, channel, 0))

#define GET_DMA_DST_TYPE(gba, channel) ((IO_DMAxCNT_H((gba), (channel)) >> 5) & 0x03)
#define GET_DMA_SRC_TYPE(gba, channel) ((IO_DMAxCNT((gba), (channel)) >> 7) & 0x03)
#define GET_DMA_START(gba, channel) ((IO_DMAxCNT_H((gba), (channel)) >> 12) & 0x03)

#define IS_DMA_REPEAT(gba, channel) CHECK_BIT(IO_DMAxCNT_H((gba), (channel)), 9)
#define IS_DMA_HALF(gba, channel) (!CHECK_BIT(IO_DMAxCNT_H((gba), (channel)), 10))
#define IS_DMA3_DRQ(gba, channel) CHECK_BIT(IO_DMAxCNT_H((gba), (channel)), 11)
#define IS_DMA_END_IRQ(gba, channel) CHECK_BIT(IO_DMAxCNT_H((gba), (channel)), 14)
#define IS_DMA_ENABLED(gba, channel) CHECK_BIT(IO_DMAxCNT_H((gba), (channel)), 15)

#define DISABLE_DMA(gba, channel) RESET_BIT(IO_DMAxCNT_H((gba), (channel)), 15)

typedef enum {
    DMA_TYPE_INCREMENT,
    DMA_TYPE_DECREMENT,
    DMA_TYPE_FIXED,
    DMA_TYPE_INCREMENT_RELOAD // TODO ???
} dma_type_t;

typedef enum {
    DMA_START_IMMEDIATELY,
    DMA_START_VBLANK,
    DMA_START_HBLANK,
    DMA_START_SPECIAL
} dma_start_t;

static inline void check_dma_requests(gba_t *gba) {
    for (uint8_t i = 0; i < GBA_DMA_CHANNEL_COUNT; i++) {
        if (!IS_DMA_ENABLED(gba, i)) {
            if (CHECK_BIT(gba->dma->active_channels, i))
                RESET_BIT(gba->dma->active_channels, i); // DMA cancel
            continue;
        }

        if (CHECK_BIT(gba->dma->active_channels, i))
            continue;

        gba_dma_channel_t *channel = &gba->dma->channel[i];

        switch (GET_DMA_START(gba, i)) {
        case DMA_START_IMMEDIATELY:
            SET_BIT(gba->dma->active_channels, i);
            puts("DMA_START_IMMEDIATELY");
            break;
        case DMA_START_VBLANK:
            todo("DMA_START_VBLANK");
            // if (PPU_IS_VBLANK(gba))
            //     SET_BIT(gba->dma->active_channels, i);
            break;
        case DMA_START_HBLANK:
            todo("DMA_START_HBLANK");
            // if (PPU_IS_HBLANK(gba))
            //     SET_BIT(gba->dma->active_channels, i);
            break;
        case DMA_START_SPECIAL:
            switch (i) {
            case 0:
                todo("DMA_START_SPECIAL for channel 0 is prohibited");
                break;
            case 1:
            case 2:
                // TODO implement sound dma
                RESET_BIT(gba->dma->active_channels, i);
                DISABLE_DMA(gba, i);
                break;
            case 3:
                todo();
                break;
            }
            break;
        default:
            todo();
            break;
        }

        uint32_t count_mask = i == 3 ? 0x07FF : 0xFFFF;
        channel->count = IO_DMAxCNT_L(gba, i) & count_mask;
        if (channel->count == 0)
            channel->count = count_mask;

        uint32_t addr_mask = i == 3 ? 0x0FFFFFFF : 0x07FFFFFF;
        channel->src = IO_DMAxSAD(gba, i) & addr_mask;
        channel->dst = IO_DMAxDAD(gba, i) & addr_mask;

        channel->is_half = IS_DMA_HALF(gba, i);
        channel->dst_increment = channel->is_half ? 2 : 4;
        channel->src_increment = channel->dst_increment;

        switch (GET_DMA_DST_TYPE(gba, i)) {
        case DMA_TYPE_INCREMENT:
            break;
        case DMA_TYPE_DECREMENT:
            channel->dst_increment = -channel->dst_increment;
            break;
        case DMA_TYPE_FIXED:
            channel->dst_increment = 0;
            break;
        case DMA_TYPE_INCREMENT_RELOAD:
            todo();
            break;
        }

        switch (GET_DMA_SRC_TYPE(gba, i)) {
        case DMA_TYPE_INCREMENT:
            break;
        case DMA_TYPE_DECREMENT:
            channel->src_increment = -channel->src_increment;
            break;
        case DMA_TYPE_FIXED:
            channel->src_increment = 0;
            break;
        case DMA_TYPE_INCREMENT_RELOAD:
            todo();
            break;
        }
    }
}

void gba_dma_step(gba_t *gba) {
    check_dma_requests(gba);

    uint8_t channel_index = stdc_first_trailing_one(gba->dma->active_channels);
    if (!channel_index)
        return;

    channel_index--;

    gba_dma_channel_t *channel = &gba->dma->channel[channel_index];

    switch (channel_index) {
    case 0:
        todo("DO DMA 0");
        break;
    case 1:
        todo("DO DMA 1");
        break;
    case 2:
        todo("DO DMA 2");
        break;
    case 3:
        break;
    default:
        todo();
        return;
    }

    if (channel->is_half)
        gba_bus_write_half(gba, channel->dst, gba_bus_read_half(gba, channel->src));
    else
        gba_bus_write_word(gba, channel->dst, gba_bus_read_word(gba, channel->src));

    channel->src += channel->src_increment;
    channel->dst += channel->dst_increment;
    channel->count--;

    if (channel->count == 0) {
        RESET_BIT(gba->dma->active_channels, channel_index);
        DISABLE_DMA(gba, channel_index);
        puts("DISABLE_DMA");
    }
}

void gba_dma_init(gba_t *gba) {
    gba->dma = xcalloc(1, sizeof(*gba->dma));
}

void gba_dma_quit(gba_t *gba) {
    free(gba->dma);
}
