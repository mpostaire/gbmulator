#pragma once

#include "../core.h"

#define GBA_SCREEN_WIDTH  240
#define GBA_SCREEN_HEIGHT 160

#define GBA_CPU_FREQ          0x1000000 // 16.8 MHz
#define GBA_FRAMES_PER_SECOND 60

#define GBA_CPU_CYCLES_PER_FRAME (GBA_CPU_FREQ / GBA_FRAMES_PER_SECOND)
#define GBA_CPU_STEPS_PER_FRAME  GBA_CPU_CYCLES_PER_FRAME

typedef struct gba_t gba_t;

void gba_step(gba_t *gba);

gba_t *gba_init(gbmulator_t *base);

void gba_quit(gba_t *gba);

void gba_print_status(gba_t *gba);

char *gba_get_rom_title(gba_t *gba);

uint16_t gba_get_joypad_state(gba_t *gba);

void gba_set_joypad_state(gba_t *gba, uint16_t state);

uint8_t *gba_get_save(gba_t *gba, size_t *save_length);

bool gba_load_save(gba_t *gba, uint8_t *save_data, size_t save_length);

uint8_t *gba_get_savestate(gba_t *gba, size_t *length, bool is_compressed);

bool gba_load_savestate(gba_t *gba, uint8_t *data, size_t length);

uint8_t *gba_get_rom(gba_t *gba, size_t *rom_size);
