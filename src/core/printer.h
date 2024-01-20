#pragma once

#include "types.h"

#define GB_PRINTER_IMG_WIDTH 160

typedef void (*gb_new_printer_line_cb_t)(const byte_t *pixels, size_t height);
typedef void (*gb_start_printing_cb_t)(const byte_t *pixels, size_t height);
typedef void (*gb_finish_printing_cb_t)(const byte_t *pixels, size_t height);

gb_printer_t *gb_printer_init(gb_new_printer_line_cb_t new_line_cb, gb_start_printing_cb_t start_printing_cb, gb_finish_printing_cb_t finish_printing_cb);

void gb_printer_quit(gb_printer_t *printer);

void gb_printer_step(gb_printer_t *printer);

byte_t gb_printer_linked_shift_bit(void *device, byte_t in_bit);

void gb_printer_linked_data_received(void *device);

byte_t *gb_printer_get_image(gb_printer_t *printer, size_t *height);

void gb_printer_clear_image(gb_printer_t *printer);
