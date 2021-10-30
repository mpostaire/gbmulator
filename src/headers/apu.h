#pragma once

typedef struct channel {
    byte_t enabled;
    byte_t dac_enabled;

    byte_t wave_position;
    byte_t duty_position;
    byte_t duty;
    int freq_timer;
    int length_timer;
    int envelope_timer;
    byte_t envelope_volume;
    int sweep_timer;
    int sweep_freq;
    byte_t sweep_enabled;

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

void apu_step(int cycles);

void apu_init(void);
