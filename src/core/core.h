#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "types.h"
#include "utils.h"
#include "gb/gb.h"
#include "gba/gba.h"

#ifndef true
#   define true 1
#endif

#ifndef false
#   define false 0
#endif

typedef struct gbmulator_t gbmulator_t;

gbmulator_t *gbmulator_init(const uint8_t *rom, size_t rom_size, gbmulator_options_t *opts);

void gbmulator_quit(gbmulator_t *emu);

void gbmulator_step(gbmulator_t *emu);

void gbmulator_run_steps(gbmulator_t *emu, uint64_t steps_limit);

void gbmulator_run_frames(gbmulator_t *emu, uint64_t frames_limit);

uint8_t *gbmulator_get_save(gbmulator_t *emu, size_t *save_length);

bool gbmulator_load_save(gbmulator_t *emu, uint8_t *save_data, size_t save_length);

uint8_t *gbmulator_get_savestate(gbmulator_t *emu, size_t *length, bool is_compressed);

bool gbmulator_load_savestate(gbmulator_t *emu, uint8_t *data, size_t length);

char *gbmulator_get_rom_title(gbmulator_t *emu);

void gbmulator_print_status(gbmulator_t *emu);

void gbmulator_set_joypad_state(gbmulator_t *emu, uint16_t state);
