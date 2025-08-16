#include "gba_priv.h"

#define IO_TMxCNT_L(gba, channel) (gba)->bus->io[tmrcnts_l[channel]]
#define IO_TMxCNT_H(gba, channel) (gba)->bus->io[tmrcnts_h[channel]]

#define TMxCNT_L_E (0x0080) // Timer Enable
#define TMxCNT_L_I (0x0040) // Timer Interrupt on overflow
#define TMxCNT_L_C (0x0004) // Timer Cascade
#define TMxCNT_L_F (0x0003) // Timer Frequency divider

#define IS_TM_COUNTUP(gba, channel) (channel != 0 && (IO_TMxCNT_H((gba), (channel)) & TMxCNT_L_C))
#define IS_TM_IRQ(gba, channel)     (IO_TMxCNT_H((gba), (channel)) & TMxCNT_L_I)
#define IS_TM_ENABLED(gba, channel) (IO_TMxCNT_H((gba), (channel)) & TMxCNT_L_E)

static const uint16_t tmrcnts_l[GBA_TMR_COUNT] = { IO_TM0CNT_L, IO_TM1CNT_L, IO_TM2CNT_L, IO_TM3CNT_L };
static const uint16_t tmrcnts_h[GBA_TMR_COUNT] = { IO_TM0CNT_H, IO_TM1CNT_H, IO_TM2CNT_H, IO_TM3CNT_H };

static const uint16_t freq_divider_values[GBA_TMR_COUNT] = { 1, 64, 256, 1024 };

static inline void tmr_step(gba_t *gba, uint8_t channel, bool *overflow) {
    if (!IS_TM_ENABLED(gba, channel)) {
        *overflow = false;
        return;
    }

    bool must_increment = *overflow;
    if (!IS_TM_COUNTUP(gba, channel))
        must_increment = gba->tmr->instance[channel].cycle++ >= gba->tmr->instance[channel].divider;

    *overflow = false;
    if (must_increment) {
        gba->tmr->instance[channel].cycle = 0;
        *overflow                         = IO_TMxCNT_L(gba, channel) == 0xFFFF;
        if (*overflow) {
            IO_TMxCNT_L(gba, channel) = gba->tmr->instance[channel].reload;
            if (IS_TM_IRQ(gba, channel))
                CPU_REQUEST_INTERRUPT(gba, IRQ_TIMER0 + channel);
        } else {
            IO_TMxCNT_L(gba, channel)++;
        }
    }
}

void gba_tmr_set(gba_t *gba, uint16_t data, uint8_t channel) {
    // Note: When simultaneously changing the start bit from 0 to 1, and setting the reload value at the same time
    // (by a single 32bit I/O operation), then the newly written reload value is recognized as new counter value.
    // --> this is implicitly implemented because 32 bit writes in IO registers is done LSB first

    bool is_enable_rising  = !(IO_TMxCNT_H(gba, channel) & TMxCNT_L_E) && (data & TMxCNT_L_E);
    bool is_enable_falling = (IO_TMxCNT_H(gba, channel) & TMxCNT_L_E) && !(data & TMxCNT_L_E);
    if (is_enable_rising) {
        gba->tmr->instance[channel].cycle = 0;
        IO_TMxCNT_L(gba, channel)         = gba->tmr->instance[channel].reload;

        for (uint8_t i = channel + 1; IS_TM_COUNTUP(gba, i) && i < GBA_TMR_COUNT; i++)
            IO_TMxCNT_L(gba, i) = gba->tmr->instance[i].reload;
    } else if (is_enable_falling) {
        gba->tmr->instance[channel].cycle = 0;
    }

    gba->tmr->instance[channel].divider = freq_divider_values[data & TMxCNT_L_F];
}

void gba_tmr_step(gba_t *gba) {
    bool overflow = false;
    for (uint8_t i = 0; i < GBA_TMR_COUNT; i++)
        tmr_step(gba, i, &overflow);
}

void gba_tmr_init(gba_t *gba) {
    gba->tmr = xcalloc(1, sizeof(*gba->tmr));

    for (uint8_t i = 0; i < GBA_TMR_COUNT; i++)
        gba->tmr->instance[i].divider = freq_divider_values[0];
}

void gba_tmr_quit(gba_t *gba) {
    free(gba->tmr);
}
