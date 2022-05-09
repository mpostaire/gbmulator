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

// TODO make both these values configurable in emulator_init()?
#define GB_APU_SAMPLE_RATE 44100
// Higher ample count means higher sound quality but lower emulation smoothness.
// 256 seems to be the safe minimum (512 for the wasm port)
#define GB_APU_SAMPLE_COUNT 512

/**
 * Runs the emulator for one cpu step.
 * @returns the ammount of cycles the emulator has run for
 */
int emulator_step(void);

/**
 * Runs the emulator for the given ammount of cpu cycles.
 * @param cycles_limit the ammount of cycles the emulator will run for
 */
void emulator_run_cycles(int cycles_limit);

/**
 * Inits the emulator.
 * @param rom_path the path to the rom the emulator will play
 * @param save_path the path to the save file of the given rom where the emulator will read (load a game save) and/or write (save a game) data
 * @param new_frame_cb the function called whenever the ppu has finished rendering a new frame
 * @param apu_samples_ready_cb the function called whenever the samples buffer of the apu is full
 */
int emulator_init(char *rom_path, char *save_path, void (*new_frame_cb)(byte_t *pixels), void (*apu_samples_ready_cb)(float *audio_buffer, int audio_buffer_size));

int emulator_init_from_data(const byte_t *rom_data, size_t size, char *save_path, void (*ppu_vblank_cb)(byte_t *pixels), void (*apu_samples_ready_cb)(float *audio_buffer, int audio_buffer_size));

/**
 * Quits the emulator gracefully (save eram into a '.sav' file, ...).
 */
void emulator_quit(void);

int emulator_start_link(const int port);

int emulator_connect_to_link(const char* address, const int port);

void emulator_joypad_press(joypad_button_t key);

void emulator_joypad_release(joypad_button_t key);

byte_t *emulator_get_save_data(size_t *save_length);

char *emulator_get_rom_title(void);

char *emulator_get_rom_title_from_data(byte_t *rom_data, size_t size);

/**
 * convert the pixels buffer from the color values of the old emulation palette to the new color values of the new palette
 */
void emulator_update_pixels_with_palette(byte_t new_palette);

byte_t emulator_get_color_palette(void);

void emulator_set_color_palette(color_palette_t palette);

byte_t *emulator_get_color_values(color_t color);

byte_t *emulator_get_pixels(void);

void emulator_set_apu_sampling_freq_multiplier(float speed);

void emulator_set_apu_sound_level(float level);
