#include "mmu.h"
#include "cpu.h"
#include "ppu.h"
#include "timer.h"
#include "joypad.h"
#include "link.h"
#include "apu.h"

void emulator_run_cycles(int cycles_limit) {
    int cycles_count = 0;
    while (cycles_count < cycles_limit) {
        // TODO make timings accurate by forcing each cpu_step() to take 4 cycles: if it's not enough to finish an instruction,
        // the next cpu_step() will resume the previous instruction. This will makes the timer "hack" (increment within a loop and not an if)
        // obsolete while allowing accurate memory timings emulation.
        int cycles = cpu_step();
        timer_step(cycles);
        link_step(cycles);
        // TODO insert mmu_step(cycles) here to implement OAM DMA transfer delay
        ppu_step(cycles);
        apu_step(cycles);

        cycles_count += cycles;
    }
}
