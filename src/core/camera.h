#pragma once

#include "types.h"

#define GB_CAMERA_N_REGS 0x36

byte_t gb_camera_read_image(gb_t *gb, uint16_t addr);

byte_t camera_read_reg(gb_t *gb, word_t address);

void camera_write_reg(gb_t *gb, word_t address, byte_t data);

void camera_step(gb_t *gb);
