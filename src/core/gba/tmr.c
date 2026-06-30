#include "gba_priv.h"

#define IO_TMxCNT_L(gba, channel) (gba)->bus.io[tmrcnts_l[channel]]
#define IO_TMxCNT_H(gba, channel) (gba)->bus.io[tmrcnts_h[channel]]

#define TMxCNT_L_E (0x0080) // Timer Enable
#define TMxCNT_L_I (0x0040) // Timer Interrupt on overflow
#define TMxCNT_L_C (0x0004) // Timer Cascade
#define TMxCNT_L_F (0x0003) // Timer Frequency divider

#define TMR_OVERFLOW_VALUE UINT64_C(0x10000)

static const uint16_t tmrcnts_l[GBA_TMR_COUNT] = { IO_TM0CNT_L, IO_TM1CNT_L, IO_TM2CNT_L, IO_TM3CNT_L };
static const uint16_t tmrcnts_h[GBA_TMR_COUNT] = { IO_TM0CNT_H, IO_TM1CNT_H, IO_TM2CNT_H, IO_TM3CNT_H };

static const uint16_t freq_divider_values[] = { 1, 64, 256, 1024 };

static inline bool is_tm_cascade(gba_t *gba, uint8_t channel) {
    assert(channel < GBA_TMR_COUNT);
    return channel != 0 && (IO_TMxCNT_H(gba, channel) & TMxCNT_L_C);
}

static inline bool is_tm_irq(gba_t *gba, uint8_t channel) {
    assert(channel < GBA_TMR_COUNT);
    return IO_TMxCNT_H(gba, channel) & TMxCNT_L_I;
}

static inline bool is_tm_enabled(gba_t *gba, uint8_t channel) {
    assert(channel < GBA_TMR_COUNT);
    return IO_TMxCNT_H(gba, channel) & TMxCNT_L_E;
}

static inline void set_overflow_event(gba_t *gba, uint8_t channel) {
    uint64_t delay  = (uint64_t) gba->tmr.instance[channel].divider * (TMR_OVERFLOW_VALUE - (uint64_t) IO_TMxCNT_L(gba, channel));
    uint64_t period = (uint64_t) gba->tmr.instance[channel].divider * (TMR_OVERFLOW_VALUE - (uint64_t) gba->tmr.instance[channel].reload);

    LOG_DEBUG("[TMR%" PRIu8 "] set_overflow_event delay=%" PRIu64 " period=%" PRIu64, channel, delay, period);

    assert(channel < GBA_TMR_COUNT);
    assert((delay > 0) && (period > 0));
    assert((GBA_SCHED_EVENT_TMR0_OVERFLOW + channel) <= GBA_SCHED_EVENT_TMR3_OVERFLOW);

    gba_sched_add(gba, GBA_SCHED_EVENT_TMR0_OVERFLOW + channel, delay, period);
}

static inline void cancel_overflow_event(gba_t *gba, uint8_t channel) {
    LOG_DEBUG("[TMR%" PRIu8 "] cancel_overflow_event", channel);

    assert(channel < GBA_TMR_COUNT);
    assert((GBA_SCHED_EVENT_TMR0_OVERFLOW + channel) <= GBA_SCHED_EVENT_TMR3_OVERFLOW);

    gba_sched_cancel(gba, GBA_SCHED_EVENT_TMR0_OVERFLOW + channel);
}

void gba_tmr_set(gba_t *gba, uint16_t data, uint8_t channel) {
    assert(channel < GBA_TMR_COUNT);

    // Note: When simultaneously changing the start bit from 0 to 1, and setting the reload value at the same time
    // (by a single 32bit I/O operation), then the newly written reload value is recognized as new counter value.
    // --> this is implicitly implemented because 32 bit writes in IO registers is done LSB first

    LOG_DEBUG(
        "[TMR%" PRIu8 "] enable=%" PRIu16 " cascade=%" PRIu16 " divider=%" PRIu16 " irq=%" PRIu16 " (raw=0x%02" PRIX16 ")",
        channel,
        (data & TMxCNT_L_E) == TMxCNT_L_E,
        (data & TMxCNT_L_C) == TMxCNT_L_C,
        data & TMxCNT_L_F,
        (data & TMxCNT_L_I) == TMxCNT_L_I,
        data
    );

    gba_tmr_sync(gba);
    gba->tmr.instance[channel].last_sync_cycle = gba->sched.cycle;

    gba->tmr.instance[channel].divider = freq_divider_values[data & TMxCNT_L_F];

    bool old_enable  = is_tm_enabled(gba, channel);
    bool old_cascade = is_tm_cascade(gba, channel);

    bool new_enable  = data & TMxCNT_L_E;
    bool new_cascade = (channel != 0) && (data & TMxCNT_L_C);

    bool is_enable_rising = !old_enable && new_enable;
    if (is_enable_rising) {
        IO_TMxCNT_L(gba, channel) = gba->tmr.instance[channel].reload;

        for (uint8_t i = channel + 1; i < GBA_TMR_COUNT && is_tm_cascade(gba, i); i++) {
            IO_TMxCNT_L(gba, i)                  = gba->tmr.instance[i].reload;
            gba->tmr.instance[i].last_sync_cycle = gba->sched.cycle;
        }
    }

    if (old_enable && !old_cascade)
        cancel_overflow_event(gba, channel);

    if (new_enable && !new_cascade)
        set_overflow_event(gba, channel);
}

static void tmr_overflow(gba_t *gba, uint8_t channel) {
    assert(channel < GBA_TMR_COUNT);

    LOG_DEBUG("[TMR%" PRIu8 "] overflow at cycle=%" PRIu64, channel, gba->sched.cycle);

    gba_tmr_sync(gba);

    // because we are in an overflow, we are sure that last_sync_cycle is the current scheduler cycle without precision loss
    gba->tmr.instance[channel].last_sync_cycle = gba->sched.cycle;

    IO_TMxCNT_L(gba, channel) = gba->tmr.instance[channel].reload;
    if (is_tm_irq(gba, channel))
        CPU_REQUEST_INTERRUPT(gba, GBA_IRQ_TIMER0 + channel);
}

void gba_tmr0_overflow(gba_t *gba) {
    tmr_overflow(gba, 0);
}

void gba_tmr1_overflow(gba_t *gba) {
    tmr_overflow(gba, 1);
}

void gba_tmr2_overflow(gba_t *gba) {
    tmr_overflow(gba, 2);
}

void gba_tmr3_overflow(gba_t *gba) {
    tmr_overflow(gba, 3);
}

void gba_tmr_sync(gba_t *gba) {
    bool is_prev_overflow = false;

    for (uint8_t i = 0; i < GBA_TMR_COUNT; i++) {
        uint64_t elapsed_cycles = gba->sched.cycle - gba->tmr.instance[i].last_sync_cycle;
        uint64_t elapsed_ticks  = elapsed_cycles / gba->tmr.instance[i].divider;

        if (!is_tm_enabled(gba, i)) {
            gba->tmr.instance[i].last_sync_cycle = gba->sched.cycle;

            is_prev_overflow = false;
        } else if (is_tm_cascade(gba, i)) {
            gba->tmr.instance[i].last_sync_cycle += elapsed_ticks * gba->sched.cycle;

            // increase cascade timers when previous timer was incremented
            if (is_prev_overflow) {
                is_prev_overflow = (IO_TMxCNT_L(gba, i) + 1) == TMR_OVERFLOW_VALUE;
                IO_TMxCNT_L(gba, i)++;
            } else {
                is_prev_overflow = false;
            }
        } else {
            // do not set last_sync_cycle to gba->sched.cycle to account for cycle losses due to integer division of elapsed_ticks
            gba->tmr.instance[i].last_sync_cycle += elapsed_ticks * gba->tmr.instance[i].divider;

            // pre-increment tmr counter must not have already overflown because overflow is handled by scheduler events
            assert(IO_TMxCNT_L(gba, i) < TMR_OVERFLOW_VALUE);
            // post-increment tmr counter can have just overflown: this means we are in the shceduler overflow event
            assert(IO_TMxCNT_L(gba, i) + elapsed_ticks <= TMR_OVERFLOW_VALUE);

            is_prev_overflow     = (IO_TMxCNT_L(gba, i) + elapsed_ticks) == TMR_OVERFLOW_VALUE;
            IO_TMxCNT_L(gba, i) += elapsed_ticks;
        }
    }
}

void gba_tmr_reset(gba_t *gba) {
    memset(&gba->tmr, 0, sizeof(gba->tmr));

    for (uint8_t i = 0; i < GBA_TMR_COUNT; i++)
        gba->tmr.instance[i].divider = freq_divider_values[0];
}
