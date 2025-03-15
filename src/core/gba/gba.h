#pragma once

#include "types.h"

void gba_step(gba_t *gba);

// TODO in the future, do not pass bios to gba_init --> embed a default custom
//      bios in the code and make bios selection optional
gba_t *gba_init(const uint8_t *rom, size_t rom_size, gba_config_t *config);

void gba_quit(gba_t *gba);

void gba_print_status(gba_t *gba);
