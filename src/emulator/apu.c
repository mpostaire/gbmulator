#include <stdlib.h>

#include "emulator_priv.h"

// FIXME rare audible pops

const byte_t duty_cycles[4][8] = {
    { 0, 0, 0, 0, 0, 0, 0, 1 },
    { 1, 0, 0, 0, 0, 0, 0, 1 },
    { 1, 0, 0, 0, 0, 1, 1, 1 },
    { 0, 1, 1, 1, 1, 1, 1, 0 }
};

static void channel_step(channel_t *c) {
    c->freq_timer--;
    if (c->freq_timer <= 0) {
        if (c->id == APU_CHANNEL_4) {
            byte_t divisor = *c->NRx3 & 0x07;
            c->freq_timer = divisor ? divisor << 4 : 8;
            c->freq_timer <<= (*c->NRx3 >> 4);

            byte_t xor_ret = (c->LFSR & 0x01) ^ ((c->LFSR & 0x02) >> 1);
            c->LFSR = (c->LFSR >> 1) | (xor_ret << 14);

            if ((*c->NRx3 >> 3) & 0x01) {
                RESET_BIT(c->LFSR, 6);
                c->LFSR |= xor_ret << 6;
            }
            return;
        }

        word_t freq = ((*c->NRx4 & 0x07) << 8) | *c->NRx3;
        if (c->id == APU_CHANNEL_3) {
            c->freq_timer = (2048 - freq) * 2;
            c->wave_position = (c->wave_position + 1) % 32;
            return;
        }
        c->freq_timer = (2048 - freq) * 4;
        c->duty_position = (c->duty_position + 1) % 8;
    }

    byte_t wave_pattern_duty = (*c->NRx1 & 0xC0) >> 6;
    c->duty = duty_cycles[wave_pattern_duty][c->duty_position];
}

static void channel_length(emulator_t *emu, channel_t *c) {
    if (!CHECK_BIT(*c->NRx4, 6)) // length enabled ?
        return;

    c->length_counter--;
    if (c->length_counter <= 0)
        APU_DISABLE_CHANNEL(emu, c->id);
}

static int calc_sweep_freq(emulator_t *emu, channel_t *c) {
    int freq = c->sweep_freq >> (*c->NRx0 & 0x07);
    if ((*c->NRx0 & 0x08) >> 3)
        freq = c->sweep_freq - freq;
    else
        freq = c->sweep_freq + freq;
    
    if (freq > 2047)
        APU_DISABLE_CHANNEL(emu, c->id);
    
    return freq;
}

static void channel_sweep(emulator_t *emu, channel_t *c) {
    c->sweep_timer--;
    if (c->sweep_timer <= 0) {
        byte_t sweep_period = (*c->NRx0 & 0x70) >> 4;
        c->sweep_timer = sweep_period > 0 ? sweep_period : 8;
    
        if (c->sweep_enabled && sweep_period > 0) {
            byte_t sweep_shift = *c->NRx0 & 0x07;
            int new_freq = calc_sweep_freq(emu, c);
            
            if (new_freq < 2048 && sweep_shift > 0) {
                c->sweep_freq = new_freq;
                
                *c->NRx3 = new_freq & 0xFF;
                *c->NRx4 = (new_freq >> 8) & 0x07;

                calc_sweep_freq(emu, c); // for the overflow check
            }
        }
    }
}

static void channel_envelope(channel_t *c) {
    if (!(*c->NRx2 & 0x07))
        return;

    c->envelope_period--;
    if (c->envelope_period <= 0) {
        c->envelope_period = *c->NRx2 & 0x07;
        byte_t direction = (*c->NRx2 & 0x08) >> 3;
        if (c->envelope_volume < 0x0F && direction)
            c->envelope_volume++;
        else if (c->envelope_volume > 0x00 && !direction)
            c->envelope_volume--;
    }
}

// TODO channel 3 ony enables channel and do nothing else?????
void apu_channel_trigger(emulator_t *emu, channel_t *c) {
    APU_ENABLE_CHANNEL(emu, c->id);
    if (c->length_counter <= 0)
        c->length_counter = c->id == APU_CHANNEL_3 ? 256 : 64;

    // // TODO find out if this should really be done
    // byte_t freq = ((*c->NRx4 & 0x03) << 8) | *c->NRx3;
    // if (c->id == CHANNEL_3) {
    //     c->freq_timer = ((2048 - freq) * 2);
    //     c->wave_position = 0;
    // } else {
    //     c->freq_timer = ((2048 - freq) * 4);
    // }

    if (c->id != APU_CHANNEL_3) {
        c->envelope_period = *c->NRx2 & 0x07;
        c->envelope_volume = *c->NRx2 >> 4;
    }

    if (c->id == APU_CHANNEL_1) {
        c->sweep_freq = ((*c->NRx4 & 0x07) << 8) | *c->NRx3;
        byte_t sweep_period = (*c->NRx0 & 0x70) >> 4;
        c->sweep_timer = sweep_period > 0 ? sweep_period : 8;
        byte_t sweep_shift = *c->NRx0 & 0x07;
        c->sweep_enabled = (sweep_period > 0) || (sweep_shift > 0);

        if (sweep_shift > 0)
            calc_sweep_freq(emu, c);
    }

    if (c->id == APU_CHANNEL_4)
        c->LFSR = 0x7FFF;

    if ((c->id == APU_CHANNEL_3 && !CHECK_BIT(*c->NRx0, 7)) || (c->id != APU_CHANNEL_3 && !(*c->NRx2 >> 3)))
        APU_DISABLE_CHANNEL(emu, c->id);
}

static float channel_dac(emulator_t *emu, channel_t *c) {
    switch (c->id) {
    case APU_CHANNEL_1:
    case APU_CHANNEL_2:
        if ((*c->NRx2 >> 3) && APU_IS_CHANNEL_ENABLED(emu, c->id)) // if dac enabled and channel enabled
            return ((c->duty * c->envelope_volume) / 7.5f) - 1.0f;
        break;
    case APU_CHANNEL_3:
        if ((*c->NRx0 >> 7) /*&& APU_IS_CHANNEL_ENABLED(c->id)*/) { // if dac enabled and channel enabled -- TODO check why channel 3 enabled flag is not working properly
            byte_t sample = emu->mmu->mem[WAVE_RAM + (c->wave_position / 2)];
            if (c->wave_position % 2 == 0) // TODO check if this works properly (I think it always reads the 2 nibbles as the same values)
                sample >>= 4;
            sample &= 0x0F;
            switch ((emu->mmu->mem[NR32] >> 5) & 0x03) {
            case 0:
                sample >>= 4; // mute
                break;
            case 2:
                sample >>= 1; // 50% volume
                break;
            case 3:
                sample >>= 2; // 25% volume
                break;
            }
            return (sample / 7.5f) - 1.0f; // divide by 7.5 then substract 1.0 to make it between -1.0 and 1.0
        }
        break;
    case APU_CHANNEL_4: // TODO works but it seems (in Tetris) this channel volume is a bit too loud
        if ((*c->NRx2 >> 3) && APU_IS_CHANNEL_ENABLED(emu, c->id)) // if dac enabled and channel enabled
            return ((!(c->LFSR & 0x01) * c->envelope_volume) / 7.5f) - 1.0f;
        break;
    }
    return 0.0f;
}

void apu_step(emulator_t *emu) {
    // TODO not sure where I saw it but apu clocking is supposed to be inferred by DIV timer?

    if (!IS_APU_ENABLED(emu))
        return;

    apu_t *apu = emu->apu;
    mmu_t *mmu = emu->mmu;

    byte_t cycles = 4; // 4 cycles per step
    while (cycles-- > 0) {
        apu->frame_sequencer_cycles_count++;
        if (apu->frame_sequencer_cycles_count >= 8192) { // 512 Hz
            apu->frame_sequencer_cycles_count = 0;
            switch (apu->frame_sequencer) {
            case 0:
                channel_length(emu, &apu->channel1);
                channel_length(emu, &apu->channel2);
                channel_length(emu, &apu->channel3);
                channel_length(emu, &apu->channel4);
                break;
            case 2:
                channel_length(emu, &apu->channel1);
                channel_sweep(emu, &apu->channel1);
                channel_length(emu, &apu->channel2);
                channel_length(emu, &apu->channel3);
                channel_length(emu, &apu->channel4);
                break;
            case 4:
                channel_length(emu, &apu->channel1);
                channel_length(emu, &apu->channel2);
                channel_length(emu, &apu->channel3);
                channel_length(emu, &apu->channel4);
                break;
            case 6:
                channel_length(emu, &apu->channel1);
                channel_sweep(emu, &apu->channel1);
                channel_length(emu, &apu->channel2);
                channel_length(emu, &apu->channel3);
                channel_length(emu, &apu->channel4);
                break;
            case 7:
                channel_envelope(&apu->channel1);
                channel_envelope(&apu->channel2);
                channel_envelope(&apu->channel4);
                break;
            }
            apu->frame_sequencer = (apu->frame_sequencer + 1) % 8;
        }

        channel_step(&apu->channel1);
        channel_step(&apu->channel2);
        channel_step(&apu->channel3);
        channel_step(&apu->channel4);

        if (emu->apu_speed > 2.0f) // don't collect samples when emulation speed increases too much
            return;

        apu->take_sample_cycles_count++;
        if (apu->take_sample_cycles_count >= (GB_CPU_FREQ / GB_APU_SAMPLE_RATE) * emu->apu_speed) { // 44100 Hz (if speed == 1.0f)
            apu->take_sample_cycles_count = 0;

            float S01_volume = ((mmu->mem[NR50] & 0x07) + 1) / 8.0f; // keep it between 0.0f and 1.0f
            float S02_volume = (((mmu->mem[NR50] & 0x70) >> 4) + 1) / 8.0f; // keep it between 0.0f and 1.0f
            float S01_output = ((CHECK_BIT(mmu->mem[NR51], APU_CHANNEL_1) ? channel_dac(emu, &apu->channel1) : 0.0f)
                                + (CHECK_BIT(mmu->mem[NR51], APU_CHANNEL_2) ? channel_dac(emu, &apu->channel2) : 0.0f)
                                + (CHECK_BIT(mmu->mem[NR51], APU_CHANNEL_3) ? channel_dac(emu, &apu->channel3) : 0.0f)
                                + (CHECK_BIT(mmu->mem[NR51], APU_CHANNEL_4) ? channel_dac(emu, &apu->channel4) : 0.0f)) / 4.0f;
            float S02_output = ((CHECK_BIT(mmu->mem[NR51], APU_CHANNEL_1 + 4) ? channel_dac(emu, &apu->channel1) : 0.0f)
                                + (CHECK_BIT(mmu->mem[NR51], APU_CHANNEL_2 + 4) ? channel_dac(emu, &apu->channel2) : 0.0f)
                                + (CHECK_BIT(mmu->mem[NR51], APU_CHANNEL_3 + 4) ? channel_dac(emu, &apu->channel3) : 0.0f)
                                + (CHECK_BIT(mmu->mem[NR51], APU_CHANNEL_4 + 4) ? channel_dac(emu, &apu->channel4) : 0.0f)) / 4.0f;

            // apply channel volume and global volume to its output
            S01_output = S01_output * S01_volume * emu->apu_sound_level;
            S02_output = S02_output * S02_volume * emu->apu_sound_level;

            switch (emu->apu_format) {
            case APU_FORMAT_F32:
                // S02 (left)
                ((float *) apu->audio_buffer)[apu->audio_buffer_index++] = S02_output;
                // S01 (right)
                ((float *) apu->audio_buffer)[apu->audio_buffer_index++] = S01_output;
                break;
            case APU_FORMAT_U8:
                // S02 (left) -->  convert from float [0, 1] to uint8 [0, 255], 128 is volume output level 0
                ((byte_t *) apu->audio_buffer)[apu->audio_buffer_index++] = (S02_output * 127) + 128;
                // S01 (right) --> convert from float [0, 1] to unt8 [0, 255], 128 is volume output level 0
                ((byte_t *) apu->audio_buffer)[apu->audio_buffer_index++] = (S01_output * 127) + 128;
                break;
            }
        }

        if (apu->audio_buffer_index >= emu->apu_sample_count) {
            apu->audio_buffer_index = 0;
            if (emu->on_apu_samples_ready)
                emu->on_apu_samples_ready(apu->audio_buffer, apu->audio_buffer_sample_size * emu->apu_sample_count);
        }
    }
}

// TODO find a way to make speed unnecessary
void apu_init(emulator_t *emu) {
    apu_t *apu = xcalloc(1, sizeof(apu_t));

    apu->audio_buffer_sample_size = emu->apu_format == APU_FORMAT_F32 ? sizeof(float) : sizeof(byte_t);
    apu->audio_buffer = xmalloc(apu->audio_buffer_sample_size * emu->apu_sample_count);

    apu->channel1 = (channel_t) {
        .NRx0 = &emu->mmu->mem[NR10],
        .NRx1 = &emu->mmu->mem[NR11],
        .NRx2 = &emu->mmu->mem[NR12],
        .NRx3 = &emu->mmu->mem[NR13],
        .NRx4 = &emu->mmu->mem[NR14],
        .id = APU_CHANNEL_1
    };
    apu->channel2 = (channel_t) { 
        .NRx1 = &emu->mmu->mem[NR21],
        .NRx2 = &emu->mmu->mem[NR22],
        .NRx3 = &emu->mmu->mem[NR23],
        .NRx4 = &emu->mmu->mem[NR24],
        .id = APU_CHANNEL_2
    };
    apu->channel3 = (channel_t) {
        .NRx0 = &emu->mmu->mem[NR30],
        .NRx1 = &emu->mmu->mem[NR31],
        .NRx2 = &emu->mmu->mem[NR32],
        .NRx3 = &emu->mmu->mem[NR33],
        .NRx4 = &emu->mmu->mem[NR34],
        .id = APU_CHANNEL_3
    };
    apu->channel4 = (channel_t) { 
        .NRx1 = &emu->mmu->mem[NR41],
        .NRx2 = &emu->mmu->mem[NR42],
        .NRx3 = &emu->mmu->mem[NR43],
        .NRx4 = &emu->mmu->mem[NR44],
        .LFSR = 0,
        .id = APU_CHANNEL_4
    };

    emu->apu = apu;
}

void apu_quit(emulator_t *emu) {
    free(emu->apu->audio_buffer);
    free(emu->apu);
}
