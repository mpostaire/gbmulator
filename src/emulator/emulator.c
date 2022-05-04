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

char *emulator_init(const char *rom_path, const char *save_path, void (*ppu_vblank_cb)(byte_t *pixels), void (*apu_samples_ready_cb)(float *audio_buffer)) {
    ppu_set_vblank_callback(ppu_vblank_cb);
    apu_init();
    apu_set_samples_ready_callback(apu_samples_ready_cb);
    mmu_init(save_path);
    return mmu_load_cartridge(rom_path);
}

void emulator_quit(void) {
    mmu_save_eram();
    link_close_connection();
}
