#include "gba_priv.h"

#define IO_TMxCNT_L(gba, channel) (gba)->bus->io_regs[IO_TM0CNT_L + ((IO_TM1CNT_L - IO_TM0CNT_L) * (channel))]
#define IO_TMxCNT_H(gba, channel) (gba)->bus->io_regs[IO_TM0CNT_H + ((IO_TM1CNT_H - IO_TM0CNT_H) * (channel))]

#define IS_TM_COUNTUP(gba, channel) (channel != 0 && CHECK_BIT(IO_TMxCNT_H((gba), (channel)), 2))
#define IS_TM_IRQ(gba, channel) CHECK_BIT(IO_TMxCNT_H((gba), (channel)), 6)
#define IS_TM_ENABLED(gba, channel) CHECK_BIT(IO_TMxCNT_H((gba), (channel)), 7)

static inline bool tmr_step(gba_t *gba, uint8_t channel, bool prev_overflow) {
    if (!IS_TM_ENABLED(gba, channel))
        return false;

    bool increment = prev_overflow;
    if (!IS_TM_COUNTUP(gba, channel))
        increment = gba->tmr->instance[channel].cycle++ >= gba->tmr->instance[channel].prescaler;

    bool overflow = false;
    if (increment) {
        gba->tmr->instance[channel].cycle = 0;
        overflow = IO_TMxCNT_L(gba, channel) == 0xFFFF;
        if (overflow) {
            IO_TMxCNT_L(gba, channel) = gba->tmr->instance[channel].reload;
            if (IS_TM_IRQ(gba, channel))
                CPU_REQUEST_INTERRUPT(gba, IRQ_TIMER0 + channel);
        } else {
            IO_TMxCNT_L(gba, channel)++;
        }
    }

    return overflow;
}

void gba_tmr_set(gba_t *gba, uint16_t data, uint8_t channel) {
    // Note: When simultaneously changing the start bit from 0 to 1, and setting the reload value at the same time
    // (by a single 32bit I/O operation), then the newly written reload value is recognized as new counter value.
    // --> this is implicitly implemented because 32 bit writes in IO registers is done LSB first

    bool is_enable_rising = !CHECK_BIT(IO_TMxCNT_H(gba, channel), 7) && CHECK_BIT(data, 7);
    bool is_enable_falling = CHECK_BIT(IO_TMxCNT_H(gba, channel), 7) && !CHECK_BIT(data, 7);
    if (is_enable_rising) {
        gba->tmr->instance[channel].cycle = 0;
        IO_TMxCNT_L(gba, channel) = gba->tmr->instance[channel].reload;

        for (uint8_t i = channel + 1; IS_TM_COUNTUP(gba, i) && i < GBA_TMR_COUNT; i++)
            IO_TMxCNT_L(gba, i) = gba->tmr->instance[i].reload;
    } else if (is_enable_falling) {
        gba->tmr->instance[channel].cycle = 0;
    }

    switch (data & 0x03) {
    case 0:
        gba->tmr->instance[channel].prescaler = 1;
        break;
    case 1:
        gba->tmr->instance[channel].prescaler = 64;
        break;
    case 2:
        gba->tmr->instance[channel].prescaler = 256;
        break;
    case 3:
        gba->tmr->instance[channel].prescaler = 1024;
        break;
    }
}

void gba_tmr_step(gba_t *gba) {
    bool overflow = false;
    for (uint8_t i = 0; i < GBA_TMR_COUNT; i++)
        overflow = tmr_step(gba, i, overflow);
}

void gba_tmr_init(gba_t *gba) {
    gba->tmr = xcalloc(1, sizeof(*gba->tmr));

    for (uint8_t i = 0; i < GBA_TMR_COUNT; i++)
        gba->tmr->instance[i].prescaler = 1;
}

void gba_tmr_quit(gba_t *gba) {
    free(gba->tmr);
}
