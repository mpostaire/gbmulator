#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../core.h"

typedef struct gbprinter_t gbprinter_t;

gbprinter_t *gbprinter_init(gbmulator_t *base);

void gbprinter_quit(gbprinter_t *printer);

void gbprinter_step(gbprinter_t *printer);

uint8_t gbprinter_link_shift_bit(gbprinter_t *printer, uint8_t in_bit);

void gbprinter_link_data_received(gbprinter_t *printer);

uint8_t *gbprinter_get_image(gbprinter_t *printer, size_t *height);

void gbprinter_clear_image(gbprinter_t *printer);
