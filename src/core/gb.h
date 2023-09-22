#pragma once

#include "types.h"
#include "utils.h"
#include "printer.h"

#define EMULATOR_NAME "GBmulator"

#define GB_SCREEN_WIDTH 160
#define GB_SCREEN_HEIGHT 144

#define GB_CPU_FREQ 4194304
#define GB_CPU_FRAMES_PER_SECONDS 60
// 4194304 cycles executed per second --> 4194304 / fps --> 4194304 / 60 == 69905 cycles per frame (the Game Boy runs at approximatively 60 fps)
#define GB_CPU_CYCLES_PER_FRAME (GB_CPU_FREQ / GB_CPU_FRAMES_PER_SECONDS)
#define GB_CPU_STEPS_PER_FRAME (GB_CPU_FREQ / 4 / GB_CPU_FRAMES_PER_SECONDS)

#define GB_APU_CHANNELS 2
#define GB_APU_SAMPLE_RATE 44100
// this is the number of samples needed per frame at a 44100Hz sample rate (735)
#define GB_APU_SAMPLES_PER_FRAME (GB_APU_SAMPLE_RATE / GB_CPU_FRAMES_PER_SECONDS)
#define GB_APU_DEFAULT_SAMPLE_COUNT 512

/**
 * Runs the emulator for one cpu step.
 * @returns the amount of cycles the emulator has run for
 */
int gb_step(gb_t *gb);

/**
 * Runs the emulator for the given amount of cpu steps.
 * @param steps_limit the amount of steps the emulator will run for
 */
static inline void gb_run_steps(gb_t *gb, long steps_limit) {
    for (long steps_count = 0; steps_count < steps_limit; steps_count++)
        gb_step(gb);
}

/**
 * Runs the emulator for the given amount of frames.
 * @param frames_limit the amount of frames the emulator will run for
 */
static inline void gb_run_frames(gb_t *gb, long frames_limit) {
    gb_run_steps(gb, frames_limit * GB_CPU_STEPS_PER_FRAME);
}

/**
 * Runs the emulator for the given amount of cpu cycles.
 * @param cycles_limit the amount of cycles the emulator will run for
 */
static inline void gb_run_cycles(gb_t *gb, long cycles_limit) {
    for (long cycles_count = 0; cycles_count < cycles_limit; cycles_count += gb_step(gb));
}

/**
 * Runs both the emulator and the linked emulator in parallel for the given amount of cpu steps.
 * @param steps_limit the amount of steps both emulators will run for.
 */
static inline void gb_linked_run_steps(gb_t *gb, gb_t *linked_gb, long steps_limit) {
    // TODO investigate link cable behaviour when double speed is on
    for (long steps_count = 0; steps_count < steps_limit; steps_count++) {
        gb_step(gb);
        gb_step(linked_gb);
    }
}

/**
 * Runs both the emulator and the linked emulator in parallel for the given amount of frames.
 * @param frames_limit the amount of frames both emulators will run for.
 */
static inline void gb_linked_run_frames(gb_t *gb, gb_t *linked_gb, long frames_limit) {
    gb_linked_run_steps(gb, linked_gb, frames_limit * GB_CPU_STEPS_PER_FRAME);
}

/**
 * Inits the emulator.
 * @param rom_data a buffer containing the rom the emulator will play.
 * @param rom_size the size of the `rom_data`.
 * @param opts the initialization options of the emulator or NULL for defaults.
 */
gb_t *gb_init(const byte_t *rom_data, size_t rom_size, gb_options_t *opts);

/**
 * Quits the emulator gracefully (save eram into a '.sav' file, ...).
 */
void gb_quit(gb_t *gb);

void gb_reset(gb_t *gb, gb_mode_t mode);

void gb_print_status(gb_t *gb);

/**
 * Connects 2 emulators through the link cable.
 * This automatically disconnects a previous link cable connection. 
 * Freeing the previously linked device is the responsibility of the caller.
 * @param gb the fist emulator.
 * @param other_emu the second emulator.
*/
void gb_link_connect(gb_t *gb, gb_t *other_emu);

void gb_link_disconnect(gb_t *gb);

/**
 * Connects an emulator and a printer through the link cable.
 * This automatically disconnects a previous link cable connection.
 * Freeing the previously linked device is the responsibility of the caller.
 * @param gb the emulator.
 * @param printer the printer.
*/
void gb_link_connect_printer(gb_t *gb, gb_printer_t *printer);

void gb_link_disconnect_printer(gb_t *gb);

void gb_joypad_press(gb_t *gb, gb_joypad_button_t key);

void gb_joypad_release(gb_t *gb, gb_joypad_button_t key);

byte_t *gb_get_save(gb_t *gb, size_t *save_length);

int gb_load_save(gb_t *gb, byte_t *save_data, size_t save_length);

byte_t *gb_get_savestate(gb_t *gb, size_t *length, byte_t compressed);

int gb_load_savestate(gb_t *gb, const byte_t *data, size_t length);

byte_t gb_get_joypad_state(gb_t *gb);

void gb_set_joypad_state(gb_t *gb, byte_t state);

/**
 * @returns the ROM title (you must not free the returned pointer).
 */
char *gb_get_rom_title(gb_t *gb);

char *gb_get_rom_title_from_data(byte_t *rom_data, size_t size);

/**
 * @returns a pointer to the ROM (you must not free the returned pointer).
 */
byte_t *gb_get_rom(gb_t *gb, size_t *rom_size);

/**
 * convert the pixels buffer from the color values of the old emulation palette to the new color values of the new palette
 */
void gb_update_pixels_with_palette(gb_t *gb, byte_t new_palette);

void gb_get_options(gb_t *gb, gb_options_t *opts);

void gb_set_options(gb_t *gb, gb_options_t *opts);

byte_t *gb_get_color_values(gb_t *gb, gb_dmg_color_t color);

byte_t *gb_get_color_values_from_palette(gb_color_palette_t palette, gb_dmg_color_t color);

byte_t *gb_get_pixels(gb_t *gb);

gb_mode_t gb_get_mode(gb_t *gb);

word_t gb_get_cartridge_checksum(gb_t *gb);

void gb_set_apu_speed(gb_t *gb, float speed);

void gb_set_apu_sound_level(gb_t *gb, float level);

void gb_set_palette(gb_t *gb, gb_color_palette_t palette);

byte_t gb_has_accelerometer(gb_t *gb);
