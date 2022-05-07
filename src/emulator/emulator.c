#include "mmu.h"
#include "cpu.h"
#include "ppu.h"
#include "timer.h"
#include "joypad.h"
#include "link.h"
#include "apu.h"
#include "types.h"
#include "emulator.h"

int emulator_step(void) {
    // TODO make timings accurate by forcing each cpu_step() to take 4 cycles: if it's not enough to finish an instruction,
    // the next cpu_step() will resume the previous instruction. This will makes the timer "hack" (increment within a loop and not an if)
    // obsolete while allowing accurate memory timings emulation.
    int cycles = cpu_step();
    timer_step(cycles);
    link_step(cycles);
    // TODO insert mmu_step(cycles) here to implement OAM DMA transfer delay
    ppu_step(cycles);
    apu_step(cycles);
    return cycles;
}

// void emulator_run_cycles(int cycles_limit) {
//     for (int cycles_count = 0; cycles_count < cycles_limit; cycles_count += emulator_step());
// }

void emulator_init(const char *rom_path, const char *save_path, void (*ppu_vblank_cb)(byte_t *pixels), void (*apu_samples_ready_cb)(float *audio_buffer, int audio_buffer_size)) {
    cpu_init();
    apu_init(1.0f, 1.0f, apu_samples_ready_cb);
    mmu_init(rom_path, save_path);
    ppu_init(ppu_vblank_cb);
    timer_init();
    link_init();
    joypad_init();
}

void emulator_quit(void) {
    mmu_save_eram();
    link_close_connection();
}

int emulator_start_link(const int port) {
    return link_start_server(port);
}

int emulator_connect_to_link(const char* address, const int port) {
    return link_connect_to_server(address, port);
}

void emulator_joypad_press(joypad_button_t key) {
    joypad_press(key);
}

void emulator_joypad_release(joypad_button_t key) {
    joypad_release(key);
}

char *emulator_get_rom_title(void) {
    return mmu.rom_title;
}

void emulator_update_pixels_with_palette(byte_t new_palette) {
    // replace old color values of the pixels with the new ones according to the new palette
    for (int i = 0; i < GB_SCREEN_WIDTH; i++) {
        for (int j = 0; j < GB_SCREEN_HEIGHT; j++) {
            byte_t *R = (ppu.pixels + ((j) * GB_SCREEN_WIDTH * 3) + ((i) * 3)) ;
            byte_t *G = (ppu.pixels + ((j) * GB_SCREEN_WIDTH * 3) + ((i) * 3) + 1);
            byte_t *B = (ppu.pixels + ((j) * GB_SCREEN_WIDTH * 3) + ((i) * 3) + 2);

            // find which color is at pixel (i,j)
            for (color_t c = WHITE; c <= BLACK; c++) {
                if (*R == ppu.color_palettes[ppu.current_color_palette][c][0] &&
                    *G == ppu.color_palettes[ppu.current_color_palette][c][1] &&
                    *B == ppu.color_palettes[ppu.current_color_palette][c][2]) {

                    // replace old color value by the new one according to the new palette
                    *R = ppu.color_palettes[new_palette][c][0];
                    *G = ppu.color_palettes[new_palette][c][1];
                    *B = ppu.color_palettes[new_palette][c][2];
                    break;
                }
            }
        }
    }
}

byte_t emulator_get_color_palette(void) {
    return ppu.current_color_palette;
}

void emulator_set_color_palette(color_palette_t palette) {
    ppu.current_color_palette = palette;
}

byte_t *emulator_get_color_values(color_t color) {
    return ppu.color_palettes[ppu.current_color_palette][color];
}

byte_t *emulator_get_pixels(void) {
    return ppu.pixels;
}

void emulator_set_apu_sampling_freq_multiplier(float speed) {
    apu.sampling_freq_multiplier = speed;
}

void emulator_set_apu_sound_level(float level) {
    apu.global_sound_level = CLAMP(level, 0.0f, 1.0f);
}
