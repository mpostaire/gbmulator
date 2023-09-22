#include <stdlib.h>

#include "gb_priv.h"
#include "serialize.h"

// timer behavior is subtle: https://gbdev.io/pandocs/Timer_Obscure_Behaviour.html

// TODO some mooneye acceptance timer tests still don't pass
// https://github.com/Gekkio/mooneye-test-suite/blob/main/acceptance/timer/tma_write_reloading.s
// https://gbdev.io/pandocs/Timer_and_Divider_Registers.html
// https://www.reddit.com/r/EmuDev/comments/acsu62/question_regarding_rapid_togglegb_test_rom_game/

static inline byte_t falling_edge_detector(gb_timer_t *timer, byte_t tima_signal) {
    byte_t out = !tima_signal && timer->falling_edge_detector_delay;
    timer->falling_edge_detector_delay = tima_signal;
    return out;
}

void timer_step(gb_t *gb) {
    gb_timer_t *timer = gb->timer;
    gb_mmu_t *mmu = gb->mmu;

    if (timer->tima_loading_value == -1) {
        timer->tima_loading_value = -2;
    } else if (timer->tima_loading_value >= 0) {
        // finish TIMA loading if TIMA was being loaded
        mmu->io_registers[TIMA - IO] = timer->tima_loading_value;
        timer->tima_loading_value = -1;
        CPU_REQUEST_INTERRUPT(gb, IRQ_TIMER);
    }

    // DIV register incremented at 16384 Hz (every 256 cycles)
    word_t div = (mmu->io_registers[DIV - IO] << 8) | mmu->io_registers[DIV_LSB - IO];
    div += 4; // each step is 4 cycles
    mmu->io_registers[DIV_LSB - IO] = div & 0x00FF;
    mmu->io_registers[DIV - IO] = div >> 8;

    byte_t tima_signal = CHECK_BIT(div, timer->tima_increase_div_bit) && CHECK_BIT(mmu->io_registers[TAC - IO], 2);

    if (falling_edge_detector(timer, tima_signal)) {
        // increase TIMA (and handle its overflow)
        if (mmu->io_registers[TIMA - IO] == 0xFF) { // TIMA is about to overflow
            // If a TMA write is executed on the same step as the content of TMA
            // is transferred to TIMA due to a timer overflow, the old value of TMA is transferred to TIMA.
            timer->tima_loading_value = timer->old_tma >= 0 ? timer->old_tma : mmu->io_registers[TMA - IO];

            // TIMA has a value of 0x00 for 4 cycles. Delay TIMA loading until next timer_step() call.
            // Timer interrupt is also delayed for 4 cycles.
            mmu->io_registers[TIMA - IO] = 0x00;
        } else {
            mmu->io_registers[TIMA - IO]++;
        }
    }

    timer->old_tma = -1;
}

void timer_init(gb_t *gb) {
    gb->timer = xcalloc(1, sizeof(gb_timer_t));
    gb->timer->tima_loading_value = -2;
    gb->timer->old_tma = -1;
}

void timer_quit(gb_t *gb) {
    free(gb->timer);
}

#define SERIALIZED_MEMBERS         \
    X(tima_increase_div_bit)       \
    X(falling_edge_detector_delay) \
    X(tima_loading_value)          \
    X(old_tma)

#define X(value) SERIALIZED_LENGTH(value);
SERIALIZED_SIZE_FUNCTION(gb_timer_t, timer,
    SERIALIZED_MEMBERS
)
#undef X

#define X(value) SERIALIZE(value);
SERIALIZER_FUNCTION(gb_timer_t, timer,
    SERIALIZED_MEMBERS
)
#undef X

#define X(value) UNSERIALIZE(value);
UNSERIALIZER_FUNCTION(gb_timer_t, timer,
    SERIALIZED_MEMBERS
)
#undef X
