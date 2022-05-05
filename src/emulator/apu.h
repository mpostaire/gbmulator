#pragma once

#include "types.h"
#include "utils.h"
#include "mmu.h"

#define APU_SAMPLE_RATE 44100
#define APU_SAMPLE_COUNT 2048

#define IS_APU_ENABLED CHECK_BIT(mmu.mem[NR52], 7)
#define APU_IS_CHANNEL_ENABLED(channel) CHECK_BIT(mmu.mem[NR52], (channel))
#define APU_ENABLE_CHANNEL(channel) SET_BIT(mmu.mem[NR52], (channel))
#define APU_DISABLE_CHANNEL(channel) RESET_BIT(mmu.mem[NR52], (channel))

enum channel_id {
    APU_CHANNEL_1,
    APU_CHANNEL_2,
    APU_CHANNEL_3,
    APU_CHANNEL_4
};

typedef struct {
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

typedef struct {
    float global_sound_level;

    int take_sample_cycles_count;
    float sampling_freq_multiplier;
    void (*samples_ready_cb)(float *audio_buffer);

    int audio_buffer_index;
    float audio_buffer[APU_SAMPLE_COUNT];

    byte_t frame_sequencer;
    int frame_sequencer_cycles_count;

    channel_t channel1;
    channel_t channel2;
    channel_t channel3;
    channel_t channel4;
} apu_t;

extern apu_t apu;

void apu_channel_trigger(channel_t *c);

/**
 * Calls samples_ready_callback given in the apu_init() function whenever there is audio to be played.
 */
void apu_step(int cycles);

/**
 * Initializes the internal state of the ppu.
 * @param global_sound_level the configurable sound output multiplier applied to all channels in stereo
 * @param sampling_freq_multiplier the configurable multiplier to increase the sampling frequency (should be the same as the emulation speed multiplier)
 * @param samples_ready_cb the function called whenever the samples buffer is full
 */
void apu_init(float global_sound_level, float sampling_freq_multiplier, void (*samples_ready_cb)(float *audio_buffer));
