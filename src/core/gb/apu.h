#pragma once

#include "gb.h"
#include "mmu.h"

#define IS_APU_ENABLED(gb) CHECK_BIT((gb)->mmu->io_registers[IO_NR52], 7)
#define APU_IS_CHANNEL_ENABLED(gb, channel) CHECK_BIT((gb)->mmu->io_registers[IO_NR52], (channel))
#define APU_ENABLE_CHANNEL(gb, channel) SET_BIT((gb)->mmu->io_registers[IO_NR52], (channel))
#define APU_DISABLE_CHANNEL(gb, channel) RESET_BIT((gb)->mmu->io_registers[IO_NR52], (channel))

typedef enum {
    APU_CHANNEL_1,
    APU_CHANNEL_2,
    APU_CHANNEL_3,
    APU_CHANNEL_4
} gb_channel_id_t;

typedef struct {
    uint8_t wave_position;
    uint8_t duty_position;
    uint8_t duty;
    int freq_timer;
    int length_counter; // length module
    int envelope_period; // envelope module
    uint8_t envelope_volume; // envelope module
    int sweep_timer; // sweep module
    int sweep_freq; // sweep module
    uint8_t sweep_enabled; // sweep module

    // registers
    uint8_t *NRx0;
    uint8_t *NRx1;
    uint8_t *NRx2;
    uint8_t *NRx3;
    uint8_t *NRx4;
    uint16_t LFSR;

    gb_channel_id_t id;
} gb_channel_t;

typedef struct {
    uint32_t take_sample_cycles_count;
    uint32_t dynamic_sampling_rate;

    uint8_t frame_sequencer;
    uint32_t frame_sequencer_cycles_count;

    gb_channel_t channel1;
    gb_channel_t channel2;
    gb_channel_t channel3;
    gb_channel_t channel4;
} apu_t;

void apu_channel_trigger(gb_t *gb, gb_channel_t *c);

/**
 * Calls samples_ready_callback given in the apu_init() function whenever there is audio to be played.
 */
void apu_step(gb_t *gb);

/**
 * Initializes the internal state of the ppu.
 */
void apu_init(gb_t *gb);

void apu_quit(gb_t *gb);
