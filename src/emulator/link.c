#include <stdlib.h>

#include "emulator.h"
#include "cpu.h"
#include "mmu.h"

// TODO this is still buggy: tetris works fine, dr mario has glitched game over screen (unrelated?) and pokemon red has glitched fight and trade
// TODO fix problems when a emulator is paused during a connection
// TODO handle disconnection

void link_set_clock(emulator_t *emu) {
    if (emu->mode == CGB) {
        // TODO handle double speed (also call this function where the double speed enable/disable is happening)
        if (CHECK_BIT(emu->mmu->mem[SC], 1))
            emu->link->clock_cycles = GB_CPU_FREQ / 8192;
        else
            emu->link->clock_cycles = GB_CPU_FREQ / 262144;
    } else {
        emu->link->clock_cycles = GB_CPU_FREQ / 8192;
    }
}

void link_init(emulator_t *emu) {
    emu->link = xcalloc(1, sizeof(link_t));
    link_set_clock(emu);
}

void link_quit(emulator_t *emu) {
    if (emu->link->other_emu)
        emulator_link_disconnect(emu);
    free(emu->link);
}

/**
 * This implementation may cause bugs if the 2 emulators wants to communicate using the internal clock
 * at the same time or during the cycles needed to shift the 8 bits of SB. This should be a rare case
 * so it's not really a problem.
 * 
 * Cause of glitches because the 2 emulators are not completely synchronized?
 * Or maybe messages aren't sent/received due to errors? <-- test this first
 * 
 * I'll have to implement a cpu sync by forcing each iteration to take 4 cycles
 * (passing memory timings of blargg's tests) and send the current cycle to the serial link for comparison to do the interrupt at the same
 * time (same cycle)
 * maybe with 512 as limit value, problems are due to the cpu not using the SB byte and being interrupted before it has the time... but unlikely
 * as the interrupts disable other interrupts as they are processed
 * 
 * If emulator on 1 computer faster than the other, may cause problems --> cpu cycle sync important?
 */
void link_step(emulator_t *emu, int cycles) {
    link_t *link = emu->link;
    mmu_t *mmu = emu->mmu;

    link->cycles_counter += cycles;

    if (link->cycles_counter >= link->clock_cycles) {
        link->cycles_counter -= link->clock_cycles; // keep leftover cycles (if any)

        if (!link->other_emu)
            return;

        // transfer requested / in progress with internal clock (this emu is the master of the connection)
        // --> the master emulator also does the work for the slave so we don't have to handle the case
        //     where this emu is the slave
        if (CHECK_BIT(mmu->mem[SC], 7) && CHECK_BIT(mmu->mem[SC], 0)) {
            if (link->bit_counter < 8) { // emulate 8 bit shifting
                link->bit_counter++;

                byte_t this_bit = GET_BIT(mmu->mem[SB], 7);
                byte_t other_bit = GET_BIT(link->other_emu->mmu->mem[SB], 7);

                // transfer other_emu bit to this emu
                mmu->mem[SB] <<= 1;
                CHANGE_BIT(mmu->mem[SB], other_bit, 0);

                // transfer this emu bit to other_emu
                link->other_emu->mmu->mem[SB] <<= 1;
                CHANGE_BIT(link->other_emu->mmu->mem[SB], this_bit, 0);
            } else { // transfer is done (all bits were shifted)
                link->bit_counter = 0;

                RESET_BIT(mmu->mem[SC], 7);
                cpu_request_interrupt(emu, IRQ_SERIAL);

                RESET_BIT(link->other_emu->mmu->mem[SC], 7);
                cpu_request_interrupt(link->other_emu, IRQ_SERIAL);
            }
        }
    }
}
