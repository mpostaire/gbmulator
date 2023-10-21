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
    GB_MODE_DMG = 1,
    GB_MODE_CGB
} gb_mode_t;

typedef struct {
    int16_t l;
    int16_t r;
} gb_apu_sample_t;

typedef void (*gb_new_frame_cb_t)(const byte_t *pixels);
typedef void (*gb_new_sample_cb_t)(const gb_apu_sample_t sample, uint32_t *dynamic_sampling_rate);
typedef void (*gb_accelerometer_request_cb_t)(double *x, double *y);
typedef byte_t (*gb_camera_capture_image_cb_t)(byte_t *image);

typedef struct {
    gb_mode_t mode; // either `GB_MODE_DMG` for original game boy emulation or `GB_MODE_CGB` for game boy color emulation
    byte_t disable_cgb_color_correction;
    gb_color_palette_t palette;
    float apu_speed;
    float apu_sound_level;
    uint32_t apu_sampling_rate;
    gb_new_frame_cb_t on_new_frame; // the function called whenever the ppu has finished rendering a new frame
    gb_new_sample_cb_t on_new_sample; // the function called whenever a new audio sample is produced by the apu
    gb_accelerometer_request_cb_t on_accelerometer_request; // the function called whenever the MBC7 latches accelerometer data
    gb_camera_capture_image_cb_t on_camera_capture_image; // the function called whenever the CAMERA requests image data
} gb_options_t;
