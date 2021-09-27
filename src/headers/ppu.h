#pragma once

#include "types.h"

extern byte_t pixels[160 * 144 * 3];

void ppu_step(int cycles);
