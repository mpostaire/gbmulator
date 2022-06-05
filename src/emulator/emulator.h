#pragma once

#include "types.h"
#include "utils.h"
#include "savestate.h"

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
#define GB_APU_SAMPLE_COUNT 512

/**
 * Runs the emulator for one cpu step.
 * @returns the ammount of cycles the emulator has run for
 */
int emulator_step(emulator_t *emu);

/**
 * Runs the emulator for the given ammount of cpu cycles.
 * @param cycles_limit the ammount of cycles the emulator will run for
 */
void emulator_run_cycles(emulator_t *emu, int cycles_limit);

/**
 * Inits the emulator.
 * @param mode either `DMG` for original game boy emulation or `CGB` for game boy color emulation
 * @param rom_path the path to the rom the emulator will play
 * @param new_frame_cb the function called whenever the ppu has finished rendering a new frame
 * @param apu_samples_ready_cb the function called whenever the samples buffer of the apu is full
 */
emulator_t *emulator_init(emulator_mode_t mode, char *rom_path, void (*new_frame_cb)(byte_t *pixels), void (*apu_samples_ready_cb)(float *audio_buffer, int audio_buffer_size));

emulator_t *emulator_init_from_data(emulator_mode_t mode, const byte_t *rom_data, size_t size, void (*ppu_vblank_cb)(byte_t *pixels), void (*apu_samples_ready_cb)(float *audio_buffer, int audio_buffer_size));

/**
 * Quits the emulator gracefully (save eram into a '.sav' file, ...).
 */
void emulator_quit(emulator_t *emu);

int emulator_start_link(emulator_t *emu, const int port);

int emulator_connect_to_link(emulator_t *emu, const char* address, const int port);

void emulator_joypad_press(emulator_t *emu, joypad_button_t key);

void emulator_joypad_release(emulator_t *emu, joypad_button_t key);

int emulator_save(emulator_t *emu, const char *path);

void emulator_load_save(emulator_t *emu, const char *path);

byte_t *emulator_get_save_data(emulator_t *emu, size_t *save_length);

void emulator_load_save_data(emulator_t *emu, byte_t *save_data, size_t save_len);

char *emulator_get_rom_path(emulator_t *emu);

char *emulator_get_rom_title(emulator_t *emu);

char *emulator_get_rom_title_from_data(byte_t *rom_data, size_t size);

byte_t *emulator_get_rom_data(emulator_t *emu, size_t *rom_size);

/**
 * convert the pixels buffer from the color values of the old emulation palette to the new color values of the new palette
 */
void emulator_update_pixels_with_palette(emulator_t *emu, byte_t new_palette);

byte_t emulator_get_color_palette(emulator_t *emu);

byte_t *emulator_get_color_values_from_palette(color_palette_t palette, dmg_color_t color);

void emulator_set_color_palette(emulator_t *emu, color_palette_t palette);

byte_t *emulator_get_color_values(emulator_t *emu, dmg_color_t color);

byte_t *emulator_get_pixels(emulator_t *emu);

void emulator_set_apu_speed(emulator_t *emu, float speed);

void emulator_set_apu_sound_level(emulator_t *emu, float level);
