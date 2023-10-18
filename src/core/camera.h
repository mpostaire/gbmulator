#pragma once

#include "types.h"

byte_t camera_read_reg(gb_t *gb, word_t address);

void camera_write_reg(gb_t *gb, word_t address, byte_t data);

void camera_step(gb_t *gb);
