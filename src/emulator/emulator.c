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

void emulator_init(const char *rom_path, const char *save_path, void (*ppu_vblank_cb)(byte_t *pixels), void (*apu_samples_ready_cb)(float *audio_buffer)) {
    cpu_init();
    timer_init();
    joypad_init();
    link_init();
    ppu_init(ppu_vblank_cb);
    apu_init(1.0f, 1.0f, apu_samples_ready_cb);
    mmu_init(save_path);
    mmu_load_cartridge(rom_path);
}

void emulator_quit(void) {
    mmu_save_eram();
    link_close_connection();
}

char *emulator_get_rom_title(void) {
    return mmu.rom_title;
}

const char *emulator_get_rom_path(void) {
    return mmu.rom_filepath;
}

byte_t emulator_get_ppu_color_palette(void) {
    return ppu.current_color_palette;
}

void emulator_set_ppu_color_palette(ppu_color_palette palette) {
    ppu.current_color_palette = palette;
}

byte_t *emulator_ppu_get_pixels(void) {
    return ppu.pixels;
}

byte_t *emulator_ppu_get_color_values(color color) {
    return ppu.color_palettes[ppu.current_color_palette][color];
}

void emulator_set_apu_sampling_freq_multiplier(float speed) {
    apu.sampling_freq_multiplier = speed;
}

void emulator_set_apu_sound_level(float level) {
    apu.global_sound_level = CLAMP(level, 0.0f, 1.0f);
}
