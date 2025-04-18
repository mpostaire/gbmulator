#pragma once

#include "types.h"
#include "../utils.h"
#include "printer.h"

#define EMULATOR_NAME "GBmulator"

#define GB_SCREEN_WIDTH 160
#define GB_SCREEN_HEIGHT 144

#define GB_CPU_FREQ 4194304
#define GB_FRAMES_PER_SECOND 60
// 4194304 cycles executed per second --> 4194304 / fps --> 4194304 / 60 == 69905 cycles per frame (the Game Boy runs at approximatively 60 fps)
#define GB_CPU_CYCLES_PER_FRAME (GB_CPU_FREQ / GB_FRAMES_PER_SECOND)
#define GB_CPU_STEPS_PER_FRAME (GB_CPU_CYCLES_PER_FRAME / 4)

#define GB_CAMERA_SENSOR_WIDTH 128
#define GB_CAMERA_SENSOR_HEIGHT 128

void gb_rewind(gb_t *gb);

/**
 * Runs the emulator for one cpu step. If `gb` is linked to another device, it is also run for one step.
 * @returns the amount of cycles the emulator has run for
 */
int gb_step(gb_t *gb);

/**
 * Runs the emulator for the given amount of cpu steps (1 step == 4 cycles).
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

int gb_is_rom_valid(const uint8_t *rom);

/**
 * Inits the emulator.
 * @param rom a buffer containing the rom the emulator will play.
 * @param rom_size the size of the `rom`.
 * @param opts the initialization options of the emulator or NULL for defaults.
 */
gb_t *gb_init(const uint8_t *rom, size_t rom_size, gb_options_t *opts);

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
 * @param other_gb the second emulator.
*/
void gb_link_connect_gb(gb_t *gb, gb_t *other_gb);

/**
 * Connects an emulator and a printer through the link cable.
 * This automatically disconnects a previous link cable connection.
 * Freeing the previously connected device is the responsibility of the caller.
 * @param gb the emulator.
 * @param printer the printer.
*/
void gb_link_connect_printer(gb_t *gb, gb_printer_t *printer);

/**
 * Disconnects any device linked to `gb`.
 * Freeing the previously connected device is the responsibility of the caller.
 * @param gb the emulator.
*/
void gb_link_disconnect(gb_t *gb);


/**
 * Connects 2 emulators through the IR sensor.
 * This automatically disconnects a previous IR connection. 
 * Freeing the previously connected device is the responsibility of the caller.
 * @param gb the fist emulator.
 * @param other_gb the second emulator.
 * @returns 1 if success, 0 if failed (`gb` and/or `other_gb` have no IR sensor)
*/
uint8_t gb_ir_connect(gb_t *gb, gb_t *other_gb);

/**
 * Disconnects any device connected with IR to `gb`.
 * Freeing the previously connected device is the responsibility of the caller.
 * @param gb the emulator.
*/
void gb_ir_disconnect(gb_t *gb);

void gb_joypad_press(gb_t *gb, gb_joypad_button_t key);

void gb_joypad_release(gb_t *gb, gb_joypad_button_t key);

uint8_t gb_get_joypad_state(gb_t *gb);

void gb_set_joypad_state(gb_t *gb, uint8_t state);

uint8_t *gb_get_save(gb_t *gb, size_t *save_length);

int gb_load_save(gb_t *gb, uint8_t *save_data, size_t save_length);

uint8_t *gb_get_savestate(gb_t *gb, size_t *length, uint8_t compressed);

int gb_load_savestate(gb_t *gb, const uint8_t *data, size_t length);

/**
 * @returns the ROM title (you must not free the returned pointer).
 */
char *gb_get_rom_title(gb_t *gb);

/**
 * @returns a pointer to the ROM (you must not free the returned pointer).
 */
uint8_t *gb_get_rom(gb_t *gb, size_t *rom_size);

void gb_get_options(gb_t *gb, gb_options_t *opts);

void gb_set_options(gb_t *gb, gb_options_t *opts);

gb_mode_t gb_is_cgb(gb_t *gb);

uint16_t gb_get_cartridge_checksum(gb_t *gb);

void gb_set_apu_speed(gb_t *gb, float speed);

void gb_set_apu_sound_level(gb_t *gb, float level);

void gb_set_palette(gb_t *gb, gb_color_palette_t palette);

uint8_t gb_has_accelerometer(gb_t *gb);

uint8_t gb_has_camera(gb_t *gb);
