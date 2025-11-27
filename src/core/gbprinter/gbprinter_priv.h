#pragma once

#include <stddef.h>
#include <stdint.h>

#include "gbprinter.h"
#include "gbprinter_defs.h"

#include "../core_priv.h"

#define GBPRINTER_CHUNK_SIZE 0x280

struct gbprinter_t {
    const gbmulator_t *base;

    uint8_t  ram[0x2000];
    uint16_t ram_len; // keep track of how much ram is used (ram writes are contiguous, starting from the previous ram_len)
    uint8_t  sb;      // Serial transfer data
    uint8_t  state;
    uint8_t  got_eof;
    uint8_t  cmd;
    uint8_t  compress_flag;
    uint16_t cmd_data_len;
    uint16_t cmd_data_recv_index;
    uint8_t  cmd_data[GBPRINTER_CHUNK_SIZE]; // max data_len is 0x280
    uint16_t checksum;
    uint8_t  status;
    uint8_t  delayed_status;

    uint16_t ram_printing_line_index; // when printing, keeps track of which line inside the ram we currently are on (reset to 0 every time the ram is cleared)
    uint32_t printing_line_time_remaining;

    struct {
        uint8_t *data;
        size_t   height;
        size_t   allocated_height; // allocated height of data (this is also the total height of the image once it has finished printing)
    } image;
};
