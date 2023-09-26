#pragma once

#include <stdint.h>
#include <stddef.h>

typedef unsigned char byte_t;
typedef signed char s_byte_t;
typedef unsigned short word_t;
typedef signed short s_word_t;

typedef struct gb_t gb_t;
typedef struct gb_printer_t gb_printer_t;

typedef enum {
    DMG_WHITE,
    DMG_LIGHT_GRAY,
    DMG_DARK_GRAY,
    DMG_BLACK
} gb_dmg_color_t;

typedef enum {
    PPU_COLOR_PALETTE_GRAY,
    PPU_COLOR_PALETTE_ORIG,
    PPU_COLOR_PALETTE_MAX // keep at the end
} gb_color_palette_t;

typedef enum {
    JOYPAD_RIGHT,
    JOYPAD_LEFT,
    JOYPAD_UP,
    JOYPAD_DOWN,
    JOYPAD_A,
    JOYPAD_B,
    JOYPAD_SELECT,
    JOYPAD_START
} gb_joypad_button_t;

typedef enum {
    DMG = 1,
    CGB
} gb_mode_t;

typedef enum {
    APU_FORMAT_F32 = 1,
    APU_FORMAT_U8,
} apu_audio_format_t;

typedef void (*gb_new_frame_cb_t)(const byte_t *pixels);
typedef void (*gb_apu_samples_ready_cb_t)(const void *audio_buffer, size_t audio_buffer_size);
typedef void (*gb_accelerometer_request_cb_t)(double *x, double *y);

typedef void (*gb_new_printer_line_cb_t)(const byte_t *pixels, size_t height);
typedef void (*gb_start_printing_cb_t)(const byte_t *pixels, size_t height);
typedef void (*gb_finish_printing_cb_t)(const byte_t *pixels, size_t height);

typedef byte_t (*linked_device_shift_bit_cb_t)(void *device, byte_t in_bit);
typedef void (*linked_device_data_received_cb_t)(void *device);

typedef struct {
    gb_mode_t mode; // either `DMG` for original game boy emulation or `CGB` for game boy color emulation
    byte_t disable_cgb_color_correction;
    gb_color_palette_t palette;
    float apu_speed;
    float apu_sound_level;
    int apu_sample_count;
    apu_audio_format_t apu_format;
    gb_new_frame_cb_t on_new_frame; // the function called whenever the ppu has finished rendering a new frame
    gb_apu_samples_ready_cb_t on_apu_samples_ready; // the function called whenever the samples buffer of the apu is full
    gb_accelerometer_request_cb_t on_accelerometer_request; // the function called whenever the MBC7 latches accelerometer data
} gb_options_t;
