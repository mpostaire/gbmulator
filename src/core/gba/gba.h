#pragma once

#include "../core.h"

#define GBA_SCREEN_WIDTH 240
#define GBA_SCREEN_HEIGHT 160

#define GBA_CPU_FREQ 0x1000000 // 16.8 Mhz
#define GBA_FRAMES_PER_SECOND 60

#define GBA_CPU_CYCLES_PER_FRAME (GBA_CPU_FREQ / GBA_FRAMES_PER_SECOND)
#define GBA_CPU_STEPS_PER_FRAME GBA_CPU_CYCLES_PER_FRAME

typedef struct gba_t gba_t;

void gba_step(gba_t *gba);

gba_t *gba_init(const uint8_t *rom, size_t rom_size, gbmulator_options_t *opts);

void gba_quit(gba_t *gba);

void gba_print_status(gba_t *gba);

char *gba_get_rom_title(gba_t *gba);

void gba_set_joypad_state(gba_t *gba, uint16_t state);
