#pragma once

#include "../core.h"

typedef struct gba_t gba_t;

uint64_t gba_step(gba_t *gba);

gba_t *gba_init(gbmulator_t *base);

void gba_quit(gba_t *gba);

void gba_print_status(gba_t *gba);

char *gba_get_rom_title(gba_t *gba);

uint16_t gba_get_joypad_state(gba_t *gba);

void gba_set_joypad_state(gba_t *gba, uint16_t state);

void gba_get_save(gba_t *gba, uint8_t *data, size_t *length);

bool gba_load_save(gba_t *gba, uint8_t *data, size_t length);

void gba_get_savestate(gba_t *gba, uint8_t *data, size_t *length);

bool gba_load_savestate(gba_t *gba, uint8_t *data, size_t length);
