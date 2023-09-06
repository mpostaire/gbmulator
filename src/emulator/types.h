#pragma once

#include <stdint.h>
#include <stddef.h>

typedef unsigned char byte_t;
typedef signed char s_byte_t;
typedef unsigned short word_t;
typedef signed short s_word_t;

typedef struct emulator_t emulator_t;

typedef enum {
    DMG_WHITE,
    DMG_LIGHT_GRAY,
    DMG_DARK_GRAY,
    DMG_BLACK
} dmg_color_t;

typedef enum {
    PPU_COLOR_PALETTE_GRAY,
    PPU_COLOR_PALETTE_ORIG,
    PPU_COLOR_PALETTE_MAX // keep at the end
} color_palette_t;

typedef enum {
    JOYPAD_RIGHT,
    JOYPAD_LEFT,
    JOYPAD_UP,
    JOYPAD_DOWN,
    JOYPAD_A,
    JOYPAD_B,
    JOYPAD_SELECT,
    JOYPAD_START
} joypad_button_t;

typedef enum {
    DMG = 1,
    CGB
} emulator_mode_t;

typedef enum {
    APU_FORMAT_F32 = 1,
    APU_FORMAT_U8,
} apu_audio_format_t;

typedef void (*on_new_frame_t)(const byte_t *pixels);
typedef void (*on_apu_samples_ready_t)(const void *audio_buffer, int audio_buffer_size);
typedef void (*on_accelerometer_request_t)(double *x, double *y);

typedef struct {
    emulator_mode_t mode; // either `DMG` for original game boy emulation or `CGB` for game boy color emulation
    byte_t disable_cgb_color_correction;
    color_palette_t palette;
    float apu_speed;
    float apu_sound_level;
    int apu_sample_count;
    apu_audio_format_t apu_format;
    on_new_frame_t on_new_frame; // the function called whenever the ppu has finished rendering a new frame
    on_apu_samples_ready_t on_apu_samples_ready; // the function called whenever the samples buffer of the apu is full
    on_accelerometer_request_t on_accelerometer_request; // the function called whenever the MBC7 latches accelerometer data
} emulator_options_t;
