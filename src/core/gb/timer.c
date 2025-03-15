#include <stdlib.h>

#include "gb_priv.h"
#include "serialize.h"

// timer behavior is subtle:
// https://github.com/Hacktix/GBEDG/blob/master/timers/index.md
// https://gbdev.io/pandocs/Timer_Obscure_Behaviour.html
// TODO wilbertpol timer_if rounds 4 and 6 not working

static inline uint8_t falling_edge_detector(gb_timer_t *timer, uint8_t tima_signal) {
    uint8_t out = !tima_signal && timer->old_tima_signal;
    timer->old_tima_signal = tima_signal;
    return out;
}

void timer_set_div_timer(gb_t *gb, uint16_t value) {
    gb_timer_t *timer = gb->timer;
    gb_mmu_t *mmu = gb->mmu;

    timer->div_timer = value;
    // DIV register incremented at 16384 Hz (every 256 cycles)
    mmu->io_registers[IO_DIV] = timer->div_timer >> 8;

    uint8_t tima_signal = CHECK_BIT(timer->div_timer, timer->tima_increase_div_bit) && CHECK_BIT(mmu->io_registers[IO_TAC], 2);
    if (falling_edge_detector(timer, tima_signal)) {
        // increase TIMA (and handle its overflow)
        if (mmu->io_registers[IO_TIMA] == 0xFF) { // TIMA is about to overflow
            // TIMA has a value of 0x00 for 4 cycles. Delay TIMA loading until next timer_step() call.
            // Timer interrupt is also delayed for 4 cycles.
            gb->timer->tima_state = TIMA_LOADING;
            mmu->io_registers[IO_TIMA] = 0x00;
        } else {
            mmu->io_registers[IO_TIMA]++;
        }
    }
}

void timer_step(gb_t *gb) {
    gb_timer_t *timer = gb->timer;
    gb_mmu_t *mmu = gb->mmu;

    switch (timer->tima_state) {
    case TIMA_LOADING_END:
        timer->tima_state = TIMA_COUNTING;
        break;
    case TIMA_LOADING:
        mmu->io_registers[IO_TIMA] = mmu->io_registers[IO_TMA];
        CPU_REQUEST_INTERRUPT(gb, IRQ_TIMER);
        timer->tima_state = TIMA_LOADING_END;
        break;
    case TIMA_LOADING_CANCELLED:
        mmu->io_registers[IO_TIMA] = timer->tima_cancelled_value;
        timer->tima_state = TIMA_LOADING_END;
        break;
    }

    timer_set_div_timer(gb, timer->div_timer + 4); // each step is 4 cycles
}

void timer_init(gb_t *gb) {
    gb->timer = xcalloc(1, sizeof(*gb->timer));
    gb->timer->tima_state = TIMA_COUNTING;
}

void timer_quit(gb_t *gb) {
    free(gb->timer);
}

#define SERIALIZED_MEMBERS   \
    X(div_timer)             \
    X(tima_increase_div_bit) \
    X(tima_state)            \
    X(tima_cancelled_value)  \
    X(old_tima_signal)

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
