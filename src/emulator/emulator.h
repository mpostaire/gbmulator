#pragma once

#include "apu.h"
#include "cpu.h"
#include "joypad.h"
#include "link.h"
#include "mmu.h"
#include "ppu.h"
#include "timer.h"
#include "types.h"
#include "utils.h"

/**
 * Runs the emulator for an ammount of cycles.
 * @param cycles_limit the ammount of cycles the emulator will run for
 */
void emulator_run_cycles(int cycles_limit);

/**
 * Inits the emulator.
 * @param rom_path
 * @param ppu_vblank_cb the function called whenever a vblank has occurred, meaning a complete frame has been rendered by the ppu
 * @param apu_samples_ready_cb the function called whenever the samples buffer of the apu is full.
 * @returns the ROM's title.
 */
char *emulator_init(const char *rom_path, void (*ppu_vblank_cb)(byte_t *pixels), void (*apu_samples_ready_cb)(float *audio_buffer));

/**
 * Quits the emulator gracefully (save eram into a '.sav' file, ...).
 */
void emulator_quit(void);
