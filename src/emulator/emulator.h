#pragma once

#include "apu.h"
#include "cpu.h"
#include "joypad.h"
#include "link.h"
#include "ppu.h"
#include "types.h"
#include "utils.h"
#include "savestate.h"

#define EMULATOR_NAME "GBmulator"

/**
 * Runs the emulator for an ammount of cycles.
 * @param cycles_limit the ammount of cycles the emulator will run for
 */
void emulator_run_cycles(int cycles_limit);

/**
 * Inits the emulator.
 * @param rom_path the path to the rom the emulator will play
 * @param save_path the path to the save file of the given rom where the emulator will read (load a game save) and/or write (save a game) data
 * @param ppu_vblank_cb the function called whenever a vblank has occurred, meaning a complete frame has been rendered by the ppu
 * @param apu_samples_ready_cb the function called whenever the samples buffer of the apu is full
 */
void emulator_init(const char *rom_path, const char *save_path, void (*ppu_vblank_cb)(byte_t *pixels), void (*apu_samples_ready_cb)(float *audio_buffer));

/**
 * Quits the emulator gracefully (save eram into a '.sav' file, ...).
 */
void emulator_quit(void);

char *emulator_get_rom_title(void);

const char *emulator_get_rom_path(void);

byte_t emulator_get_ppu_color_palette(void);

void emulator_set_ppu_color_palette(ppu_color_palette palette);

byte_t *emulator_ppu_get_pixels(void);

byte_t *emulator_ppu_get_color_values(color color);

void emulator_set_apu_sampling_freq_multiplier(float speed);

void emulator_set_apu_sound_level(float level);
