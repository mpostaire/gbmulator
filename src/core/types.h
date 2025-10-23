#pragma once

#include <stddef.h>
#include <stdint.h>

#include "core_defs.h"

typedef struct __attribute__((packed)) {
    char identifier[sizeof(SAVESTATE_STRING)];
    char rom_title[16];
    bool is_compressed;
    uint8_t mode;
    uint8_t data[];
} gbmulator_savestate_t;

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
} gbmulator_joypad_button_t;

typedef void (*gbmulator_new_line_cb_t)(const uint8_t *pixels, size_t current_height, size_t total_height);
typedef void (*gbmulator_new_frame_cb_t)(const uint8_t *pixels);
typedef void (*gbmulator_new_sample_cb_t)(const gbmulator_apu_sample_t sample, uint32_t *dynamic_sampling_rate);
typedef void (*gbmulator_accelerometer_request_cb_t)(double *x, double *y);
typedef uint8_t (*gbmulator_camera_capture_image_cb_t)(uint8_t *image);

typedef enum {
    GBMULATOR_MODE_GB,
    GBMULATOR_MODE_GBC,
    GBMULATOR_MODE_GBA,
    GBMULATOR_MODE_GBPRINTER
} gbmulator_mode_t;

typedef enum {
    GBMULATOR_LINK_CABLE,
    GBMULATOR_LINK_IR
} gbmulator_link_t;

typedef enum {
    GBMULATOR_PERIPHERAL_CAMERA,
    GBMULATOR_PERIPHERAL_ACCELEROMETER
} gbmulator_peripheral_t;

typedef struct {
    gbmulator_mode_t mode;
    uint8_t *rom; // TODO this with gbmulator_get_options is ambiguous because it may be invalid after init: use gbmulator_get_rom
    size_t rom_size; // TODO this with gbmulator_get_options is ambiguous because it may be invalid after init: use gbmulator_get_rom

    gb_color_palette_t palette;
    float apu_speed;
    uint32_t apu_sampling_rate;

    gbmulator_new_line_cb_t on_new_line; // TODO for now only used by gbprinter but it should be available or gb/gbc/gba
    gbmulator_new_frame_cb_t on_new_frame; // the function called whenever the ppu has finished rendering a new frame
    gbmulator_new_sample_cb_t on_new_sample; // the function called whenever a new audio sample is produced by the apu
    gbmulator_accelerometer_request_cb_t on_accelerometer_request; // the function called whenever the MBC7 latches accelerometer data
    gbmulator_camera_capture_image_cb_t on_camera_capture_image; // the function called whenever the CAMERA requests image data
} gbmulator_options_t;
