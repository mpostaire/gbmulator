#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "types.h"
#include "utils.h"

#include "gb/gb_defs.h"
#include "gba/gba_defs.h"
#include "gbprinter/gbprinter_defs.h"

#define EMULATOR_NAME "GBmulator"

#ifndef true
#   define true 1
#endif

#ifndef false
#   define false 0
#endif

typedef struct gbmulator_t gbmulator_t;

gbmulator_t *gbmulator_init(const gbmulator_options_t *opts);

void gbmulator_quit(gbmulator_t *emu);

bool gbmulator_reset(gbmulator_t *emu, gbmulator_mode_t new_mode);

void gbmulator_rewind(gbmulator_t *emu, uint64_t frame);

void gbmulator_step(gbmulator_t *emu);

void gbmulator_run_steps(gbmulator_t *emu, uint64_t steps_limit);

void gbmulator_run_frames(gbmulator_t *emu, uint64_t frames_limit);

uint8_t *gbmulator_get_save(gbmulator_t *emu, size_t *save_length);

bool gbmulator_load_save(gbmulator_t *emu, uint8_t *save_data, size_t save_length);

uint8_t *gbmulator_get_savestate(gbmulator_t *emu, size_t *length, bool is_compressed);

bool gbmulator_load_savestate(gbmulator_t *emu, uint8_t *data, size_t length);

void gbmulator_get_options(gbmulator_t *emu, gbmulator_options_t *opts);

void gbmulator_set_options(gbmulator_t *emu, const gbmulator_options_t *opts);

char *gbmulator_get_rom_title(gbmulator_t *emu);

void gbmulator_print_status(gbmulator_t *emu);

uint16_t gbmulator_get_joypad_state(gbmulator_t *emu);

void gbmulator_set_joypad_state(gbmulator_t *emu, uint16_t state);

uint8_t *gbmulator_get_rom(gbmulator_t *emu, size_t *rom_size);

/**
 * Connects 2 emulators through the link cable.
 * This automatically disconnects a previous link cable connection. 
 * Freeing the previously linked device is the responsibility of the caller.
 * @param emu the fist emulator.
 * @param other the second emulator.
 * @param type the type of link.
*/
void gbmulator_link_connect(gbmulator_t *emu, gbmulator_t *other, gbmulator_link_t type);

/**
 * Disconnects any device linked to `emu`.
 * Freeing the previously connected device is the responsibility of the caller.
 * @param emu the emulator.
 * @param type the type of link.
*/
void gbmulator_link_disconnect(gbmulator_t *emu, gbmulator_link_t type);

uint16_t gbmulator_get_rom_checksum(gbmulator_t *emu);

bool gbmulator_has_peripheral(gbmulator_t *emu, gbmulator_peripheral_t peripheral);

void gbmulator_set_apu_speed(gbmulator_t *emu, float speed);

void gbmulator_set_palette(gbmulator_t *emu, gb_color_palette_t palette);
