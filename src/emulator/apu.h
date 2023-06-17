#pragma once

#include "types.h"
#include "mmu.h"

#define IS_APU_ENABLED(emu_ptr) CHECK_BIT((emu_ptr)->mmu->mem[NR52], 7)
#define APU_IS_CHANNEL_ENABLED(emu_ptr, channel) CHECK_BIT((emu_ptr)->mmu->mem[NR52], (channel))
#define APU_ENABLE_CHANNEL(emu_ptr, channel) SET_BIT((emu_ptr)->mmu->mem[NR52], (channel))
#define APU_DISABLE_CHANNEL(emu_ptr, channel) RESET_BIT((emu_ptr)->mmu->mem[NR52], (channel))

typedef enum {
    APU_CHANNEL_1,
    APU_CHANNEL_2,
    APU_CHANNEL_3,
    APU_CHANNEL_4
} channel_id_t;

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

    channel_id_t id;
} channel_t;

typedef struct {
    int take_sample_cycles_count;

    int audio_buffer_index;
    size_t audio_buffer_sample_size;
    void *audio_buffer;

    byte_t frame_sequencer;
    int frame_sequencer_cycles_count;

    channel_t channel1;
    channel_t channel2;
    channel_t channel3;
    channel_t channel4;
} apu_t;

void apu_channel_trigger(emulator_t *emu, channel_t *c);

/**
 * Calls samples_ready_callback given in the apu_init() function whenever there is audio to be played.
 */
void apu_step(emulator_t *emu);

/**
 * Initializes the internal state of the ppu.
 */
void apu_init(emulator_t *emu);

void apu_quit(emulator_t *emu);
