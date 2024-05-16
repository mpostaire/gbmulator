#pragma once

#include "types.h"

void gba_step(gba_t *gba);

// TODO in the future, do not pass bios to gba_init --> embed a default custom
//      bios in the code and make bios selection optional
gba_t *gba_init(uint8_t *rom, size_t rom_size, uint8_t *bios, size_t bios_size);

void gba_quit(gba_t *gba);

bool gba_is_rom_valid(uint8_t *rom, size_t rom_size);
