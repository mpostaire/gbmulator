#pragma once

#include "types.h"

#define APU_SAMPLE_RATE 44100
#define APU_SAMPLE_COUNT 2048

typedef struct channel {
    byte_t enabled;

    byte_t wave_position;
    byte_t duty_position;
    byte_t duty;
    int freq_timer;
    int length_counter; // length module
    int envelope_period; // envelope module
    byte_t envelope_volume; // envelope module
    int sweep_timer; // sweep module
    int sweep_freq; // sweep module
    byte_t sweep_enabled; // sweep module

    // registers
    byte_t *NRx0;
    byte_t *NRx1;
    byte_t *NRx2;
    byte_t *NRx3;
    byte_t *NRx4;
    word_t LFSR;

    byte_t id;
} channel_t;

extern channel_t channel1;
extern channel_t channel2;
extern channel_t channel3;
extern channel_t channel4;

extern byte_t apu_enabled;

void apu_channel_trigger(channel_t *c);

/**
 * Calls samples_ready_callback given in the apu_init() function whenever there is audio to be played.
 */
void apu_step(int cycles);

void apu_set_sampling_speed_multiplier(float speed);

void apu_set_global_sound_level(float);

float apu_get_global_sound_level(void);

void apu_set_samples_ready_callback(void (*samples_ready_callback)(float *audio_buffer));
