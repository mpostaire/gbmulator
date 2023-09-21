#pragma once

#include "types.h"

#define GB_PRINTER_IMG_WIDTH 160

gb_printer_t *gb_printer_init(on_new_printer_line_t new_line_cb, on_start_printing_t start_printing_cb, on_finish_printing_t finish_printing_cb);

void gb_printer_quit(gb_printer_t *printer);

void gb_printer_step(gb_printer_t *printer);

void gb_printer_data_received(gb_printer_t *printer);

byte_t *gb_printer_get_image(gb_printer_t *printer, size_t *height);

void gb_printer_clear_image(gb_printer_t *printer);
