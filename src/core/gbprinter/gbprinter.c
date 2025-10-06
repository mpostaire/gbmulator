#include <stdlib.h>

#include "gbprinter_priv.h"

#include "../gb/gb_defs.h"
#include "../utils.h"
#include "../core_priv.h"

// Because the documentation is incomplete and sources contradict each other, the implementation is based on SameBoy:
// https://github.com/LIJI32/SameBoy/blob/master/Core/printer.c 

// TODO printing pictures taken with the gb camera isn't working

#define MAGIC_1 0x88
#define MAGIC_2 0x33

#define STATUS_IDLE 0x00
#define STATUS_DONE 0x04
#define STATUS_PRINTING 0x06
#define STATUS_READY 0x08

#define PRINTER_IMG_WIDTH 160
#define PRINTER_LINE_SIZE (PRINTER_IMG_WIDTH * 2)
#define N_LINES_TO_PRINT(printer) ((printer)->ram_len / (PRINTER_LINE_SIZE / 8))
#define LINE_PRINTING_TIME (GB_CPU_FREQ / 8) // 8 lines per second

typedef enum {
    WAIT_MAGIC_1,
    WAIT_MAGIC_2,
    WAIT_COMMAND,
    WAIT_COMPRESS_FLAG,
    WAIT_DATA_LEN_LO,
    WAIT_DATA_LEN_HI,
    WAIT_DATA,
    WAIT_CHKSUM_LO,
    WAIT_CHKSUM_HI,
    SEND_STATUS
} printer_state_t;

typedef enum {
    INIT = 0x01,
    PRINT = 0x02,
    FILL = 0x04,
    STATUS = 0x0F
} printer_cmd_t;

typedef enum {
    COLOR_WHITE,
    COLOR_LIGHT_GRAY,
    COLOR_DARK_GRAY,
    COLOR_BLACK
} gbprinter_color_t;

gbprinter_t *gbprinter_init(gbmulator_t *base) {
    return xcalloc(1, sizeof(gbprinter_t));
}

void gbprinter_quit(gbprinter_t *printer) {
    free(printer->image.data);
    free(printer);
}

uint8_t *gbprinter_get_image(gbprinter_t *printer, size_t *height) {
    *height = printer->image.height;
    return printer->image.data;
}

void gbprinter_clear_image(gbprinter_t *printer) {
    printer->image.height = 0;
    printer->image.allocated_height = 0;
    free(printer->image.data);
    printer->image.data = NULL; // important to avoid the xrealloc of the PRINT command to cause a double free (because it would realloc on an freed pointer)
}

static inline void render_line(gbprinter_t *printer) {
    for (uint8_t tile_number = 0; tile_number < 20; tile_number++) { // PRINTER_IMG_WIDTH / 8 == 20 tiles per line
        uint16_t line_offset = ((printer->ram_printing_line_index % 8) * 2) + ((printer->ram_printing_line_index / 8) * PRINTER_LINE_SIZE);
        uint16_t tileslice_index = line_offset + (tile_number * 16);

        uint8_t tileslice_lo = printer->ram[tileslice_index];
        uint8_t tileslice_hi = printer->ram[tileslice_index + 1];

        for (uint8_t bit_number = 0; bit_number < 8; bit_number++) {
            uint8_t bit_lo = GET_BIT(tileslice_lo, 7 - bit_number);
            uint8_t bit_hi = GET_BIT(tileslice_hi, 7 - bit_number);
            uint8_t color = (bit_hi << 1) | bit_lo;

            // apply palette
            uint8_t palette = printer->cmd_data[2];
            color = (palette >> (color << 1)) & 0x03;

            int x = (tile_number * 8) + bit_number;
            int image_data_index = (printer->image.height * PRINTER_IMG_WIDTH * 4) + (x * 4);
            switch (color) {
            case COLOR_WHITE:
                printer->image.data[image_data_index] = 0xFF;
                printer->image.data[image_data_index + 1] = 0xFF;
                printer->image.data[image_data_index + 2] = 0xFF;
                break;
            case COLOR_LIGHT_GRAY:
                printer->image.data[image_data_index] = 0xAA;
                printer->image.data[image_data_index + 1] = 0xAA;
                printer->image.data[image_data_index + 2] = 0xAA;
                break;
            case COLOR_DARK_GRAY:
                printer->image.data[image_data_index] = 0x55;
                printer->image.data[image_data_index + 1] = 0x55;
                printer->image.data[image_data_index + 2] = 0x55;
                break;
            case COLOR_BLACK:
                printer->image.data[image_data_index] = 0x00;
                printer->image.data[image_data_index + 1] = 0x00;
                printer->image.data[image_data_index + 2] = 0x00;
                break;
            }
            printer->image.data[image_data_index + 3] = 0xFF;
        }
    }

    printer->ram_printing_line_index++;
    printer->image.height++;
}

void gbprinter_step(gbprinter_t *printer) {
    if (printer->status != STATUS_PRINTING)
        return;

    printer->printing_line_time_remaining -= 4;
    if (printer->printing_line_time_remaining > 0)
        return;

    render_line(printer);

    if (printer->base->opts.on_new_line)
        printer->base->opts.on_new_line(printer->image.data, printer->image.height, printer->image.allocated_height);

    if (printer->image.height == printer->image.allocated_height)
        printer->status = STATUS_DONE;
    else
        printer->printing_line_time_remaining = LINE_PRINTING_TIME;
}

static inline uint8_t check_ram_len(gbprinter_t *printer) {
    if (printer->ram_len >= sizeof(printer->ram)) {
        // not sure about this...
        SET_BIT(printer->status, 4); // bit 4: packet error
        SET_BIT(printer->status, 2); // bit 2: image data full (ram full)
        return 0;
    }
    return 1;
}

static void exec_command(gbprinter_t *printer) {
    switch (printer->cmd) {
    case INIT:
        printer->status = STATUS_IDLE;
        printer->ram_len = 0;
        printer->ram_printing_line_index = 0;
        printer->got_eof = 0;
        break;
    case PRINT:
        if (!printer->got_eof || printer->cmd_data_len != 4) {
            SET_BIT(printer->status, 4); // bit 4: packet error
            break;
        }

        printer->got_eof = 0;
        // Pokemon tcg sometimes sends an INIT, FILL eof and PRINT.
        // There are no documentation for a PRINT of size < GBPRINTER_CHUNK_SIZE so we just allow it but we don't actually print anyting.
        if (printer->ram_len < GBPRINTER_CHUNK_SIZE)
            break;

        // printer->cmd_data[0] is always 0x01 and has unknown purpose
        // uint8_t margins = printer->cmd_data[1];
        // uint8_t exposure = printer->cmd_data[3] & 0x7F; // 0x00 -> -25% darkness, 0x7F -> +25% darkness

        printer->image.allocated_height = printer->image.height + N_LINES_TO_PRINT(printer);
        printer->image.data = xrealloc(printer->image.data, printer->image.allocated_height * PRINTER_IMG_WIDTH * 4);

        printer->printing_line_time_remaining = LINE_PRINTING_TIME;
        printer->delayed_status = STATUS_PRINTING;
        break;
    case FILL:
        printer->got_eof = printer->cmd_data_len == 0;
        if (!printer->got_eof) {
            for (uint16_t i = 0; i < printer->cmd_data_len; i++) {
                if (printer->compress_flag) {
                    uint8_t is_compressed_run = GET_BIT(printer->cmd_data[i], 7);
                    uint16_t run_length = (printer->cmd_data[i] & 0x7F) + 1 + is_compressed_run;
                    if (is_compressed_run) {
                        i++;
                        for (uint16_t j = 0; j < run_length; j++) {
                            if (!check_ram_len(printer)) break;
                            printer->ram[printer->ram_len++] = printer->cmd_data[i];
                        }
                    } else {
                        for (uint16_t j = 0; j < run_length; j++) {
                            if (!check_ram_len(printer)) break;
                            printer->ram[printer->ram_len++] = printer->cmd_data[++i];
                        }
                    }
                } else {
                    if (!check_ram_len(printer)) break;
                    printer->ram[printer->ram_len++] = printer->cmd_data[i];
                }
            }
        }

        if (printer->status & 0xF1) {
            // error or invalid checksum
        } else if (printer->ram_len >= GBPRINTER_CHUNK_SIZE) {
            printer->status = STATUS_READY; // bit 3: unprocessed data in memory (ready to print)
        }
        break;
    case STATUS:
        break;
    default:
        SET_BIT(printer->status, 4); // bit 4: packet error
        break;
    }

    if (printer->status == STATUS_DONE)
        printer->status = STATUS_IDLE;
}

uint8_t gbprinter_link_shift_bit(gbprinter_t *printer, uint8_t in_bit) {
    uint8_t out_bit = GET_BIT(printer->sb, 7);
    printer->sb <<= 1;
    CHANGE_BIT(printer->sb, 0, in_bit);
    return out_bit;
}

void gbprinter_link_data_received(gbprinter_t *printer) {
    switch (printer->state) {
    case WAIT_MAGIC_1:
        if (printer->sb == MAGIC_1)
            printer->state = WAIT_MAGIC_2;
        RESET_BIT(printer->status, 0); // reset invalid checksum bit
        printer->sb = 0x00;
        printer->checksum = 0;
        break;
    case WAIT_MAGIC_2:
        if (printer->sb == MAGIC_2) {
            printer->state = WAIT_COMMAND;
        } else {
            printer->state = WAIT_MAGIC_1;
            SET_BIT(printer->status, 4); // bit 4: packet error
        }
        printer->sb = 0x00;
        break;
    case WAIT_COMMAND:
        printer->cmd = printer->sb;
        printer->checksum += printer->sb;
        printer->sb = 0x00;
        printer->state = WAIT_COMPRESS_FLAG;
        break;
    case WAIT_COMPRESS_FLAG:
        printer->compress_flag = printer->sb & 0x01;
        printer->checksum += printer->sb;
        printer->sb = 0x00;
        printer->state = WAIT_DATA_LEN_LO;
        break;
    case WAIT_DATA_LEN_LO:
        printer->cmd_data_len = printer->sb;
        printer->checksum += printer->sb;
        printer->sb = 0x00;
        printer->state = WAIT_DATA_LEN_HI;
        break;
    case WAIT_DATA_LEN_HI:
        printer->cmd_data_len |= printer->sb << 8;
        printer->cmd_data_len = MIN(printer->cmd_data_len, GBPRINTER_CHUNK_SIZE);
        printer->cmd_data_recv_index = 0;
        printer->checksum += printer->sb;
        printer->sb = 0x00;
        printer->state = printer->cmd_data_len > 0 ? WAIT_DATA : WAIT_CHKSUM_LO;
        break;
    case WAIT_DATA:
        printer->cmd_data[printer->cmd_data_recv_index++] = printer->sb;
        printer->checksum += printer->sb;
        printer->sb = 0x00;
        if (printer->cmd_data_recv_index == printer->cmd_data_len)
            printer->state = WAIT_CHKSUM_LO;
        break;
    case WAIT_CHKSUM_LO:
        printer->checksum ^= printer->sb;
        printer->sb = 0x00;
        printer->state = WAIT_CHKSUM_HI;
        break;
    case WAIT_CHKSUM_HI:
        printer->checksum ^= printer->sb << 8;
        CHANGE_BIT(printer->status, 0, !!printer->checksum);

        printer->sb = 0x81; // send alive byte
        printer->state = SEND_STATUS;
        break;
    case SEND_STATUS:
        exec_command(printer);
        printer->sb = printer->status; // send status
        if (printer->delayed_status) {
            printer->status = printer->delayed_status;
            printer->delayed_status = 0;
        }
        printer->state = WAIT_MAGIC_1;
        break;
    }
}
