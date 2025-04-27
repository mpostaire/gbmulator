#pragma once

#include <stdint.h>

typedef struct {
    int16_t l;
    int16_t r;
} gbmulator_apu_sample_t;

typedef enum {
    PPU_COLOR_PALETTE_GRAY,
    PPU_COLOR_PALETTE_ORIG,
    PPU_COLOR_PALETTE_MAX // keep at the end
} gb_color_palette_t; // TODO rename?

typedef enum {
    JOYPAD_A,
    JOYPAD_B,
    JOYPAD_SELECT,
    JOYPAD_START,
    JOYPAD_RIGHT,
    JOYPAD_LEFT,
    JOYPAD_UP,
    JOYPAD_DOWN,
    JOYPAD_R,
    JOYPAD_L
} joypad_button_t;

typedef void (*gbmulator_new_frame_cb_t)(const uint8_t *pixels);
typedef void (*gbmulator_new_sample_cb_t)(const gbmulator_apu_sample_t sample, uint32_t *dynamic_sampling_rate);
typedef void (*gbmulator_accelerometer_request_cb_t)(double *x, double *y);
typedef uint8_t (*gbmulator_camera_capture_image_cb_t)(uint8_t *image);

typedef enum {
    GBMULATOR_MODE_GB,
    GBMULATOR_MODE_GBC,
    GBMULATOR_MODE_GBA
} gbmulator_mode_t;

typedef struct {
    gbmulator_mode_t mode;

    bool disable_color_correction;
    gb_color_palette_t palette;
    float apu_speed;
    uint32_t apu_sampling_rate;
    gbmulator_new_frame_cb_t on_new_frame; // the function called whenever the ppu has finished rendering a new frame
    gbmulator_new_sample_cb_t on_new_sample; // the function called whenever a new audio sample is produced by the apu
    gbmulator_accelerometer_request_cb_t on_accelerometer_request; // the function called whenever the MBC7 latches accelerometer data
    gbmulator_camera_capture_image_cb_t on_camera_capture_image; // the function called whenever the CAMERA requests image data
} gbmulator_options_t;
