#include <stdlib.h>

#include "emulator.h"
#include "timer.h"
#include "mmu.h"
#include "cpu.h"

// TODO timer actual behavior is more subtle: https://gbdev.io/pandocs/Timer_Obscure_Behaviour.html

void timer_step(emulator_t *emu) {
    gbtimer_t *timer = emu->timer;
    mmu_t *mmu = emu->mmu;

    // DIV register incremented at 16384 Hz (every 256 cycles)
    word_t div = (mmu->mem[DIV] << 8) | mmu->mem[DIV_LSB];
    div += 4; // each step is 4 cycles
    mmu->mem[DIV_LSB] = div & 0x00FF;
    mmu->mem[DIV] = div >> 8;

    // if timer is enabled
    if (CHECK_BIT(mmu->mem[TAC], 2)) {
        timer->tima_counter += 4;
        if (timer->tima_counter >= timer->max_tima_cycles) {
            timer->tima_counter -= timer->max_tima_cycles;
            if (mmu->mem[TIMA] == 0xFF) { // about to overflow
                // TODO If a TMA write is executed on the same cycle as the content of TMA is transferred to TIMA due to a timer overflow,
                //      the old value is transferred to TIMA.
                mmu->mem[TIMA] = mmu->mem[TMA];
                CPU_REQUEST_INTERRUPT(emu, IRQ_TIMER);
            } else {
                mmu->mem[TIMA]++;
            }
        }
    }
}

void timer_init(emulator_t *emu) {
    emu->timer = xcalloc(1, sizeof(gbtimer_t));
}

void timer_quit(emulator_t *emu) {
    free(emu->timer);
}

size_t timer_serialized_length(UNUSED emulator_t *emu) {
    return 4;
}

byte_t *timer_serialize(emulator_t *emu, size_t *size) {
    *size = timer_serialized_length(emu);
    byte_t *buf = xmalloc(*size);
    memcpy(buf, &emu->timer->max_tima_cycles, 2);
    memcpy(&buf[2], &emu->timer->tima_counter, 2);
    return buf;
}

void timer_unserialize(emulator_t *emu, byte_t *buf) {
    memcpy(&emu->timer->max_tima_cycles, buf, 2);
    memcpy(&emu->timer->tima_counter, &buf[2], 2);
}
