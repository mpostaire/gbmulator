#pragma once

#include "emulator.h"
#include "cpu.h"
#include "mmu.h"
#include "dma.h"
#include "ppu.h"
#include "apu.h"
#include "timer.h"
#include "joypad.h"
#include "link.h"
#include "printer.h"

struct emulator_t {
    emulator_mode_t mode;
    byte_t disable_cgb_color_correction;
    byte_t exit_on_invalid_opcode;
    float apu_sound_level;
    float apu_speed;
    int apu_sample_count;
    apu_audio_format_t apu_format;
    byte_t dmg_palette;
    on_new_frame_t on_new_frame;
    on_apu_samples_ready_t on_apu_samples_ready;
    on_accelerometer_request_t on_accelerometer_request;

    char rom_title[17];

    cpu_t *cpu;
    mmu_t *mmu;
    ppu_t *ppu;
    apu_t *apu;
    gbtimer_t *timer;
    joypad_t *joypad;
    link_t *link;
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
    byte_t cmd_data[0x280]; // max data_len is 0x280
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

    on_new_printer_line_t on_new_line; // the function called whenever the printer finished printing a line
    on_start_printing_t on_start_printing;
    on_finish_printing_t on_finish_printing;
};
