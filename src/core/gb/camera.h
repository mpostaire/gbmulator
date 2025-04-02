#pragma once

#include "gb.h"

#define GB_CAMERA_N_REGS 0x36

uint8_t gb_camera_read_image(gb_t *gb, uint16_t addr);

uint8_t camera_read_reg(gb_t *gb, uint16_t address);

void camera_write_reg(gb_t *gb, uint16_t address, uint8_t data);

void camera_step(gb_t *gb);
