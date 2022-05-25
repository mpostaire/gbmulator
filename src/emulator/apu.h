#pragma once

#include "types.h"
#include "mmu.h"

#define IS_APU_ENABLED(emu_ptr) CHECK_BIT((emu_ptr)->mmu->mem[NR52], 7)
#define APU_IS_CHANNEL_ENABLED(emu_ptr, channel) CHECK_BIT((emu_ptr)->mmu->mem[NR52], (channel))
#define APU_ENABLE_CHANNEL(emu_ptr, channel) SET_BIT((emu_ptr)->mmu->mem[NR52], (channel))
#define APU_DISABLE_CHANNEL(emu_ptr, channel) RESET_BIT((emu_ptr)->mmu->mem[NR52], (channel))

void apu_channel_trigger(emulator_t *emu, channel_t *c);

/**
 * Calls samples_ready_callback given in the apu_init() function whenever there is audio to be played.
 */
void apu_step(emulator_t *emu, int cycles);

/**
 * Initializes the internal state of the ppu.
 * @param global_sound_level the configurable sound output multiplier applied to all channels in stereo
 * @param speed the configurable multiplier to increase the sampling rate (should be the same as the emulation speed multiplier)
 * @param samples_ready_cb the function called whenever the samples buffer is full
 */
void apu_init(emulator_t *emu, float global_sound_level, float speed, void (*samples_ready_cb)(float *audio_buffer, int audio_buffer_size));

void apu_quit(emulator_t *emu);
