#include <stdlib.h>

#include "emulator.h"
#include "timer.h"
#include "mmu.h"
#include "cpu.h"

// TODO timer actual behavior is more subtle: https://gbdev.io/pandocs/Timer_Obscure_Behaviour.html

void timer_step(emulator_t *emu, int cycles) {
    gbtimer_t *timer = emu->timer;
    mmu_t *mmu = emu->mmu;

    // DIV register incremented at 16384 Hz (every 256 cycles)
    timer->div_counter += cycles;
    if (timer->div_counter >= 256) {
        timer->div_counter -= 256;
        mmu->mem[DIV]++; // force increment (mem_write would set this address to 0)
    }

    // if timer is enabled
    if (CHECK_BIT(mmu->mem[TAC], 2)) {
        // get frequency of TIMA increment
        int max_cycles = mmu->mem[TAC] & 0x03; // get bits 1-0
        switch (max_cycles) {
            case 0: max_cycles = 1024; break;
            case 1: max_cycles = 16; break;
            case 2: max_cycles = 64; break;
            case 3: max_cycles = 256; break;
        }

        timer->tima_counter += cycles;
        // Loop here to increment the timer multiple times in case a cpu instruction was longer than max_cycles.
        // div_counter is not affected by this as no cpu instruction takes 256 cycles.
        // This is needed as this is not an accurate emulator: the timer is not called every 4 cycles (causing wrong memory timings, ...).
        while (timer->tima_counter >= max_cycles) {
            timer->tima_counter -= max_cycles;

            if (mmu->mem[TIMA] == 0xFF) { // about to overflow
                // TODO If a TMA write is executed on the same cycle as the content of TMA is transferred to TIMA due to a timer overflow,
                //      the old value is transferred to TIMA.
                mmu->mem[TIMA] = mmu->mem[TMA];
                cpu_request_interrupt(emu, IRQ_TIMER);
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
