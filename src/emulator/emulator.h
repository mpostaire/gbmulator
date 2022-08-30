#pragma once

#include "types.h"
#include "utils.h"

#define EMULATOR_NAME "GBmulator"

#define GB_SCREEN_WIDTH 160
#define GB_SCREEN_HEIGHT 144

#define GB_CPU_FREQ 4194304
// 4194304 cycles executed per second --> 4194304 / fps --> 4194304 / 60 == 69905 cycles per frame (the Game Boy runs at approximatively 60 fps)
#define GB_CPU_CYCLES_PER_FRAME (GB_CPU_FREQ / 60)

#define GB_APU_CHANNELS 2
#define GB_APU_SAMPLE_RATE 44100
// this is the number of samples needed per frame at a 44100Hz sample rate (735)
#define GB_APU_SAMPLES_PER_FRAME (GB_APU_SAMPLE_RATE / 60)
#define GB_APU_DEFAULT_SAMPLE_COUNT 512

/**
 * Runs the emulator for one cpu step.
 * @returns the ammount of cycles the emulator has run for
 */
int emulator_step(emulator_t *emu);

/**
 * Runs the emulator for the given ammount of cpu cycles.
 * @param cycles_limit the ammount of cycles the emulator will run for
 */
inline void emulator_run_cycles(emulator_t *emu, int cycles_limit) {
    for (int cycles_count = 0; cycles_count < cycles_limit; cycles_count += emulator_step(emu));
}

/**
 * Inits the emulator.
 * @param rom_data a buffer containing the rom the emulator will play
 * @param rom_size the size of the `rom_data`
 * @param opts the initialization options of the emulator or NULL for defaults.
 */
emulator_t *emulator_init(const byte_t *rom_data, size_t rom_size, emulator_options_t *opts);

/**
 * Quits the emulator gracefully (save eram into a '.sav' file, ...).
 */
void emulator_quit(emulator_t *emu);

void emulator_reset(emulator_t *emu, emulator_mode_t mode);

void emulator_print_status(emulator_t *emu);

void emulator_link_connect(emulator_t *emu, emulator_t *other_emu);

void emulator_link_disconnect(emulator_t *emu);

void emulator_joypad_press(emulator_t *emu, joypad_button_t key);

void emulator_joypad_release(emulator_t *emu, joypad_button_t key);

byte_t *emulator_get_save(emulator_t *emu, size_t *save_length);

int emulator_load_save(emulator_t *emu, byte_t *save_data, size_t save_length);

byte_t *emulator_get_savestate(emulator_t *emu, size_t *length, byte_t compressed);

int emulator_load_savestate(emulator_t *emu, const byte_t *data, size_t length);

byte_t emulator_get_joypad_state(emulator_t *emu);

void emulator_set_joypad_state(emulator_t *emu, byte_t state);

/**
 * @returns the ROM title (you must not free the returned pointer).
 */
char *emulator_get_rom_title(emulator_t *emu);

char *emulator_get_rom_title_from_data(byte_t *rom_data, size_t size);

/**
 * @returns a pointer to the ROM (you must not free the returned pointer).
 */
byte_t *emulator_get_rom(emulator_t *emu, size_t *rom_size);

/**
 * convert the pixels buffer from the color values of the old emulation palette to the new color values of the new palette
 */
void emulator_update_pixels_with_palette(emulator_t *emu, byte_t new_palette);

void emulator_get_options(emulator_t *emu, emulator_options_t *opts);

void emulator_set_options(emulator_t *emu, emulator_options_t *opts);

byte_t *emulator_get_color_values(emulator_t *emu, dmg_color_t color);

byte_t *emulator_get_color_values_from_palette(color_palette_t palette, dmg_color_t color);

byte_t *emulator_get_pixels(emulator_t *emu);

void emulator_set_apu_speed(emulator_t *emu, float speed);

void emulator_set_apu_sound_level(emulator_t *emu, float level);

void emulator_set_palette(emulator_t *emu, color_palette_t palette);
