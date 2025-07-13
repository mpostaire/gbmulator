#include <stdbit.h>

#include "gba_priv.h"

#define IO_DMAxCNT_H(gba, channel) (gba)->bus->io[(IO_DMA0CNT_H + ((IO_DMA1CNT_H - IO_DMA0CNT_H) * (channel)))]
#define IO_DMAxCNT_L(gba, channel) (gba)->bus->io[(IO_DMA0CNT_L + ((IO_DMA1CNT_L - IO_DMA0CNT_L) * (channel)))]
#define IO_DMAxCNT(gba, channel) ((IO_DMAxCNT_H(gba, channel) << 8) | IO_DMAxCNT_L(gba, channel))

#define _IO_DMAxSAD(gba, channel, half) (gba)->bus->io[(IO_DMA0SAD + ((IO_DMA1SAD - IO_DMA0SAD) * (channel))) + (half)]
#define IO_DMAxSAD(gba, channel) ((_IO_DMAxSAD(gba, channel, 1) << 16) | _IO_DMAxSAD(gba, channel, 0))

#define _IO_DMAxDAD(gba, channel, half) (gba)->bus->io[(IO_DMA0DAD + ((IO_DMA1DAD - IO_DMA0DAD) * (channel))) + (half)]
#define IO_DMAxDAD(gba, channel) ((_IO_DMAxDAD(gba, channel, 1) << 16) | _IO_DMAxDAD(gba, channel, 0))

#define GET_DMA_DST_TYPE(gba, channel) ((IO_DMAxCNT_H((gba), (channel)) >> 5) & 0x03)
#define GET_DMA_SRC_TYPE(gba, channel) ((IO_DMAxCNT_H((gba), (channel)) >> 7) & 0x03)
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
            // disable a DMA channel if it was previously enabled and has not started yet
            if (CHECK_BIT(gba->dma->active_channels, i))
                RESET_BIT(gba->dma->active_channels, i);
            else if (CHECK_BIT(gba->dma->pending_channels, i))
                RESET_BIT(gba->dma->pending_channels, i);
            else if (CHECK_BIT(gba->dma->pending_channels, i + 4))
                RESET_BIT(gba->dma->pending_channels, i + 4);

            continue;
        }

        if (CHECK_BIT(gba->dma->active_channels, i) || CHECK_BIT(gba->dma->pending_channels, i) || CHECK_BIT(gba->dma->pending_channels, i + 4))
            continue;

        // TODO when DMA enable bit written, DMA takes 2 cycles of setup (4 when both src and dst are in gamepak ROM)
        // TODO When accessing OAM (7000000h) or OBJ VRAM (6010000h) by HBlank Timing, then the "H-Blank Interval Free" bit in DISPCNT register must be set.

        gba_dma_channel_t *channel = &gba->dma->channel[i];

        switch (GET_DMA_START(gba, i)) {
        case DMA_START_IMMEDIATELY:
            SET_BIT(gba->dma->active_channels, i);
            break;
        case DMA_START_VBLANK:
            SET_BIT(gba->dma->pending_channels, i + 4);
            break;
        case DMA_START_HBLANK:
            SET_BIT(gba->dma->pending_channels, i);
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

        uint32_t src_addr_mask = 0x0FFFFFFF;
        uint32_t dst_addr_mask = 0x07FFFFFF;
        if (i == 0)
            src_addr_mask = 0x07FFFFFF;
        else if (i == 3)
            dst_addr_mask = 0x0FFFFFFF;

        channel->src = IO_DMAxSAD(gba, i);
        channel->src &= src_addr_mask;
        channel->dst = IO_DMAxDAD(gba, i);
        channel->dst &= dst_addr_mask;

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

        channel->bus_access = BUS_ACCESS_N;

        static const char *starts[] = {
            [DMA_START_IMMEDIATELY] = "IMMEDIATELY",
            [DMA_START_VBLANK] = "VBLANK",
            [DMA_START_HBLANK] = "HBLANK",
            [DMA_START_SPECIAL] = "SPECIAL"
        };
        printf("[DMA%u] START %s src=0x%08X (%d) dst=0x%08X (%d) cnt=%u sz=%u repeat=%u\n", i, starts[GET_DMA_START(gba, i)], channel->src, channel->src_increment, channel->dst, channel->dst_increment, channel->count, channel->is_half ? 2 : 4, IS_DMA_REPEAT(gba, i));
    }
}

void gba_dma_step(gba_t *gba) {
    check_dma_requests(gba);

    uint8_t channel_index = 0;

    if (gba->dma->pending_channels) {
        switch (gba->ppu->period) {
        case GBA_PPU_PERIOD_HBLANK:
            channel_index = stdc_first_trailing_one(gba->dma->pending_channels & 0x0Fu);
            break;
        case GBA_PPU_PERIOD_VBLANK:
            channel_index = stdc_first_trailing_one((gba->dma->pending_channels >> 4u) & 0x0Fu);
            break;
        default:
            break;
        }

        if (!channel_index)
            return;
        channel_index--;

        RESET_BIT(gba->dma->pending_channels, channel_index);
        SET_BIT(gba->dma->active_channels, channel_index);
    }

    channel_index = stdc_first_trailing_one(gba->dma->active_channels);
    if (!channel_index)
        return;

    channel_index--;

    gba_dma_channel_t *channel = &gba->dma->channel[channel_index];

    if (channel->is_half) {
        uint16_t data;
        if (channel->src < BUS_EWRAM) {
            data = gba->dma->data_read_latch;
        } else {
            data = _gba_bus_read_half(gba, channel->bus_access, channel->src);
            gba->dma->data_read_latch = (data << 16) | data;
        }

        _gba_bus_write_half(gba, channel->bus_access, channel->dst, data);

        printf("[DMA%u]     0x%08X = 0x%04X (0x%08X)\n", channel_index, channel->dst, data, channel->src);
    } else {
        uint32_t data;
        if (channel->src < BUS_EWRAM) {
            data = gba->dma->data_read_latch;
        } else {
            data = _gba_bus_read_word(gba, channel->bus_access, channel->src);
            gba->dma->data_read_latch = data;
        }

        _gba_bus_write_word(gba, channel->bus_access, channel->dst, data);

        printf("[DMA%u]     0x%08X = 0x%08X (0x%08X)\n", channel_index, channel->dst, data, channel->src);
    }

    channel->bus_access = BUS_ACCESS_S;

    channel->src += channel->src_increment;
    channel->dst += channel->dst_increment;
    channel->count--;

    if (channel->count == 0) {
        if (IS_DMA_REPEAT(gba, channel_index)) // TODO repeat bit should be cached?
            todo("DMA REPEAT");

        RESET_BIT(gba->dma->active_channels, channel_index);
        DISABLE_DMA(gba, channel_index);
        printf("[DMA%u] END\n", channel_index);
    }
}

void gba_dma_init(gba_t *gba) {
    gba->dma = xcalloc(1, sizeof(*gba->dma));
}

void gba_dma_quit(gba_t *gba) {
    free(gba->dma);
}
