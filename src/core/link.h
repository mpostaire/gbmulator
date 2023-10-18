#pragma once

#include "types.h"

typedef byte_t (*linked_device_shift_bit_cb_t)(void *device, byte_t in_bit);
typedef void (*linked_device_data_received_cb_t)(void *device);

typedef struct {
    word_t cycles;
    word_t max_clock_cycles;
    byte_t bit_shift_counter;

    struct {
        void *device;
        enum {
            LINK_TYPE_NONE,
            LINK_TYPE_GB,
            LINK_TYPE_PRINTER
        } type;
        linked_device_shift_bit_cb_t shift_bit;
        linked_device_data_received_cb_t data_received;
    } linked_device;
} gb_link_t;

void link_set_clock(gb_t *gb);

void link_init(gb_t *gb);

void link_quit(gb_t *gb);

void link_step(gb_t *gb);

byte_t gb_linked_shift_bit(void *device, byte_t in_bit);

void gb_linked_data_received(void *device);
