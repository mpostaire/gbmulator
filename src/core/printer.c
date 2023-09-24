#include <stdlib.h>

#include "gb_priv.h"

// Because the documentation is incomplete and sources contradict each other, the implementation is based on SameBoy:
// https://github.com/LIJI32/SameBoy/blob/master/Core/printer.c 

#define MAGIC_1 0x88
#define MAGIC_2 0x33

#define STATUS_IDLE 0x00
#define STATUS_DONE 0x04
#define STATUS_PRINTING 0x06
#define STATUS_READY 0x08

#define PRINTER_LINE_SIZE (GB_PRINTER_IMG_WIDTH * 2)
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

gb_printer_t *gb_printer_init(gb_new_printer_line_cb_t new_line_cb, gb_start_printing_cb_t start_printing_cb, gb_finish_printing_cb_t finish_printing_cb) {
    gb_printer_t *printer = xcalloc(1, sizeof(gb_printer_t));
    printer->on_new_line = new_line_cb;
    printer->on_start_printing = start_printing_cb;
    printer->on_finish_printing = finish_printing_cb;
    return printer;
}

void gb_printer_quit(gb_printer_t *printer) {
    free(printer->image.data);
    free(printer);
}

byte_t *gb_printer_get_image(gb_printer_t *printer, size_t *height) {
    *height = printer->image.height;
    return printer->image.data;
}

void gb_printer_clear_image(gb_printer_t *printer) {
    printer->image.height = 0;
    printer->image.allocated_height = 0;
    free(printer->image.data);
    printer->image.data = NULL; // important to avoid the xrealloc of the PRINT command to cause a double free (because it would realloc on an freed pointer)
}

static inline void render_line(gb_printer_t *printer) {
    for (byte_t tile_number = 0; tile_number < 20; tile_number++) { // GB_PRINTER_IMG_WIDTH / 8 == 20 tiles per line
        word_t line_offset = ((printer->ram_printing_line_index % 8) * 2) + ((printer->ram_printing_line_index / 8) * PRINTER_LINE_SIZE);
        word_t tileslice_index = line_offset + (tile_number * 16);

        byte_t tileslice_lo = printer->ram[tileslice_index];
        byte_t tileslice_hi = printer->ram[tileslice_index + 1];

        for (byte_t bit_number = 0; bit_number < 8; bit_number++) {
            byte_t bit_lo = GET_BIT(tileslice_lo, 7 - bit_number);
            byte_t bit_hi = GET_BIT(tileslice_hi, 7 - bit_number);
            byte_t color = (bit_hi << 1) | bit_lo;

            // apply palette
            byte_t palette = printer->cmd_data[2];
            color = (palette >> (color << 1)) & 0x03;

            int x = (tile_number * 8) + bit_number;
            int image_data_index = (printer->image.height * GB_PRINTER_IMG_WIDTH * 4) + (x * 4);
            switch (color) {
            case DMG_WHITE:
                printer->image.data[image_data_index] = 0xFF;
                printer->image.data[image_data_index + 1] = 0xFF;
                printer->image.data[image_data_index + 2] = 0xFF;
                break;
            case DMG_LIGHT_GRAY:
                printer->image.data[image_data_index] = 0xAA;
                printer->image.data[image_data_index + 1] = 0xAA;
                printer->image.data[image_data_index + 2] = 0xAA;
                break;
            case DMG_DARK_GRAY:
                printer->image.data[image_data_index] = 0x55;
                printer->image.data[image_data_index + 1] = 0x55;
                printer->image.data[image_data_index + 2] = 0x55;
                break;
            case DMG_BLACK:
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

    if (printer->on_new_line)
        printer->on_new_line(printer->image.data, printer->image.height);
}

void gb_printer_step(gb_printer_t *printer) {
    if (printer->status != STATUS_PRINTING)
        return;

    printer->printing_line_time_remaining -= 4;
    if (printer->printing_line_time_remaining > 0)
        return;

    // int n_chunks = printer->ram_len / sizeof(printer->cmd_data);
    // int n_printed_chunks = printer->image.height / 16; // how many chunks were previously printed
    // int n_printed_lines = printer->image.height; // how many lines were previously printed

    render_line(printer);

    if (printer->image.height == printer->image.allocated_height) {
        printer->status = STATUS_DONE;
        if (printer->on_finish_printing)
            printer->on_finish_printing(printer->image.data, printer->image.height);
    } else {
        printer->printing_line_time_remaining = LINE_PRINTING_TIME;
    }
}

static void exec_command(gb_printer_t *printer) {
    // TODO can't INIT or FILL while PRINTing??
    switch (printer->cmd) {
    case INIT:
        // TODO don't need to memset to 0, setting the ram_len to 0 is equivalent as it will overwrite everything.
        memset(printer->ram, 0, sizeof(printer->ram));
        printer->status = STATUS_IDLE;
        printer->ram_len = 0;
        printer->ram_printing_line_index = 0;
        break;
    case PRINT:
        if (!printer->got_eof)
            break; // TODO set some status bit here?
        if (printer->cmd_data_len != 4)
            break; // TODO set some status bit here?
        // TODO only print if printer->status == STATUS_READY??

        // printer->cmd_data[0] is always 0x01 and has unknown purpose
        byte_t margins = printer->cmd_data[1];
        byte_t exposure = printer->cmd_data[3] & 0x7F; // 0x00 -> -25% darkness, 0x7F -> +25% darkness

        printer->image.allocated_height = printer->image.height + N_LINES_TO_PRINT(printer);
        printer->image.data = xrealloc(printer->image.data, printer->image.allocated_height * GB_PRINTER_IMG_WIDTH * 4);

        printer->printing_line_time_remaining = LINE_PRINTING_TIME;
        printer->delayed_status = STATUS_PRINTING;
        printer->ram_printing_line_index = 0; // TODO here?

        eprintf("TODO printing request margins=0x%02X (%d), exposure=0x%02X (%d)\n", margins, margins, exposure, exposure);

        if (printer->on_start_printing)
            printer->on_start_printing(printer->image.data, printer->image.height);
        break;
    case FILL:
        if (printer->cmd_data_len == 0) {
            printer->got_eof = 1;
        } else {
            for (word_t i = 0; i < printer->cmd_data_len; i++) {
                if (printer->ram_len >= sizeof(printer->ram)) {
                    // TODO? SET_BIT(printer->status, 4); // bit 4: packet error // TODO macro
                    // SET_BIT(printer->status, 2); // bit 2: image data full (ram full) // TODO macro
                    // TODO rename ram_len into ram_len
                    eprintf("ram_len %d\n", printer->ram_len);
                    break;
                }
                printer->ram[printer->ram_len++] = printer->cmd_data[i];
            }
        }

        if ((printer->status /*& 0xF0*/) || CHECK_BIT(printer->status, 0)) {
            // error or invalid checksum
        } else if (printer->ram_len >= sizeof(printer->cmd_data)) {
            printer->status = STATUS_READY; // bit 3: unprocessed data in memory (ready to print)
        }
        break;
    case STATUS:
        break;
    default:
        SET_BIT(printer->status, 4); // bit 4: packet error
        break;
    }

    if (printer->status == STATUS_DONE) // TODO is this right?
        printer->status = STATUS_IDLE;

    // printf("status=0x%02X\n", printer->status);
}

byte_t gb_printer_linked_shift_bit(void *device, byte_t in_bit) {
    gb_printer_t *printer = device;
    byte_t out_bit = GET_BIT(printer->sb, 7);
    printer->sb <<= 1;
    CHANGE_BIT(printer->sb, 0, in_bit);
    return out_bit;
}

void gb_printer_linked_data_received(void *device) {
    gb_printer_t *printer = device;

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
        // TODO
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
        printer->cmd_data_len = MIN(printer->cmd_data_len, sizeof(printer->cmd_data));
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
