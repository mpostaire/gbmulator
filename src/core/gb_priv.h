#pragma once

#include "gb.h"
#include "cpu.h"
#include "mmu.h"
#include "dma.h"
#include "ppu.h"
#include "apu.h"
#include "timer.h"
#include "joypad.h"
#include "link.h"
#include "printer.h"
#include "camera.h"

#define PRINTER_CHUNK_SIZE 0x280

struct gb_t {
    gb_mode_t mode;
    byte_t disable_cgb_color_correction;
    byte_t dmg_palette;
    float apu_sound_level;
    float apu_speed;
    uint32_t apu_sampling_rate;
    gb_new_frame_cb_t on_new_frame;
    gb_new_sample_cb_t on_new_sample;
    gb_accelerometer_request_cb_t on_accelerometer_request;
    gb_camera_capture_image_cb_t on_camera_capture_image;

    char rom_title[17];

    gb_cpu_t *cpu;
    gb_mmu_t *mmu;
    gb_ppu_t *ppu;
    apu_t *apu;
    gb_timer_t *timer;
    gb_joypad_t *joypad;
    gb_link_t *link;
    gb_t *ir_gb; // this is a pointer to an IR-linked emulator or NULL
};

struct gb_printer_t {
    byte_t ram[0x2000];
    word_t ram_len; // keep track of how much ram is used (ram writes are contiguous, starting from the previous ram_len)
    byte_t sb; // Serial transfer data
    byte_t state;
    byte_t got_eof;
    byte_t cmd;
    byte_t compress_flag;
    word_t cmd_data_len;
    word_t cmd_data_recv_index;
    byte_t cmd_data[PRINTER_CHUNK_SIZE]; // max data_len is 0x280
    word_t checksum;
    byte_t status;
    byte_t delayed_status;

    word_t ram_printing_line_index; // when printing, keeps track of which line inside the ram we currently are on (reset to 0 every time the ram is cleared)
    uint32_t printing_line_time_remaining;

    struct {
        byte_t *data;
        size_t height;
        size_t allocated_height; // allocated height of data (this is also the total height of the image once it has finished printing)
    } image;

    gb_new_printer_line_cb_t on_new_line; // the function called whenever the printer finished printing a line
    gb_start_printing_cb_t on_start_printing;
    gb_finish_printing_cb_t on_finish_printing;
};
