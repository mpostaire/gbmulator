#include <stdlib.h>

#include "gb_priv.h"

// FIXME rare audible pops

const byte_t duty_cycles[4][8] = {
    { 0, 0, 0, 0, 0, 0, 0, 1 },
    { 1, 0, 0, 0, 0, 0, 0, 1 },
    { 1, 0, 0, 0, 0, 1, 1, 1 },
    { 0, 1, 1, 1, 1, 1, 1, 0 }
};

static void channel_step(gb_channel_t *c) {
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

static void channel_length(gb_t *gb, gb_channel_t *c) {
    if (!CHECK_BIT(*c->NRx4, 6)) // length enabled ?
        return;

    c->length_counter--;
    if (c->length_counter <= 0)
        APU_DISABLE_CHANNEL(gb, c->id);
}

static int calc_sweep_freq(gb_t *gb, gb_channel_t *c) {
    int freq = c->sweep_freq >> (*c->NRx0 & 0x07);
    if ((*c->NRx0 & 0x08) >> 3)
        freq = c->sweep_freq - freq;
    else
        freq = c->sweep_freq + freq;
    
    if (freq > 2047)
        APU_DISABLE_CHANNEL(gb, c->id);
    
    return freq;
}

static void channel_sweep(gb_t *gb, gb_channel_t *c) {
    c->sweep_timer--;
    if (c->sweep_timer <= 0) {
        byte_t sweep_period = (*c->NRx0 & 0x70) >> 4;
        c->sweep_timer = sweep_period > 0 ? sweep_period : 8;
    
        if (c->sweep_enabled && sweep_period > 0) {
            byte_t sweep_shift = *c->NRx0 & 0x07;
            int new_freq = calc_sweep_freq(gb, c);
            
            if (new_freq < 2048 && sweep_shift > 0) {
                c->sweep_freq = new_freq;
                
                *c->NRx3 = new_freq & 0xFF;
                *c->NRx4 = (new_freq >> 8) & 0x07;

                calc_sweep_freq(gb, c); // for the overflow check
            }
        }
    }
}

static void channel_envelope(gb_channel_t *c) {
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
void apu_channel_trigger(gb_t *gb, gb_channel_t *c) {
    APU_ENABLE_CHANNEL(gb, c->id);
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
            calc_sweep_freq(gb, c);
    }

    if (c->id == APU_CHANNEL_4)
        c->LFSR = 0x7FFF;

    if ((c->id == APU_CHANNEL_3 && !CHECK_BIT(*c->NRx0, 7)) || (c->id != APU_CHANNEL_3 && !(*c->NRx2 >> 3)))
        APU_DISABLE_CHANNEL(gb, c->id);
}

static float channel_dac(gb_t *gb, gb_channel_t *c) {
    switch (c->id) {
    case APU_CHANNEL_1:
    case APU_CHANNEL_2:
        if ((*c->NRx2 >> 3) && APU_IS_CHANNEL_ENABLED(gb, c->id)) // if dac enabled and channel enabled
            return ((c->duty * c->envelope_volume) / 7.5f) - 1.0f;
        break;
    case APU_CHANNEL_3:
        if ((*c->NRx0 >> 7) /*&& APU_IS_CHANNEL_ENABLED(c->id)*/) { // if dac enabled and channel enabled -- TODO check why channel 3 enabled flag is not working properly
            byte_t sample = gb->mmu->io_registers[(WAVE_RAM - IO) + (c->wave_position / 2)];
            if (c->wave_position % 2 == 0) // TODO check if this works properly (I think it always reads the 2 nibbles as the same values)
                sample >>= 4;
            sample &= 0x0F;
            switch ((gb->mmu->io_registers[NR32 - IO] >> 5) & 0x03) {
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
        if ((*c->NRx2 >> 3) && APU_IS_CHANNEL_ENABLED(gb, c->id)) // if dac enabled and channel enabled
            return ((!(c->LFSR & 0x01) * c->envelope_volume) / 7.5f) - 1.0f;
        break;
    }
    return 0.0f;
}

void apu_step(gb_t *gb) {
    // TODO not sure where I saw it but apu clocking is supposed to be inferred by DIV timer?

    if (!IS_APU_ENABLED(gb))
        return;

    apu_t *apu = gb->apu;
    gb_mmu_t *mmu = gb->mmu;

    for (byte_t cycles = 0; cycles < 4; cycles++) { // 4 cycles per step
        apu->frame_sequencer_cycles_count++;
        if (apu->frame_sequencer_cycles_count >= 8192) { // 512 Hz
            apu->frame_sequencer_cycles_count = 0;
            switch (apu->frame_sequencer) {
            case 0:
                channel_length(gb, &apu->channel1);
                channel_length(gb, &apu->channel2);
                channel_length(gb, &apu->channel3);
                channel_length(gb, &apu->channel4);
                break;
            case 2:
                channel_length(gb, &apu->channel1);
                channel_sweep(gb, &apu->channel1);
                channel_length(gb, &apu->channel2);
                channel_length(gb, &apu->channel3);
                channel_length(gb, &apu->channel4);
                break;
            case 4:
                channel_length(gb, &apu->channel1);
                channel_length(gb, &apu->channel2);
                channel_length(gb, &apu->channel3);
                channel_length(gb, &apu->channel4);
                break;
            case 6:
                channel_length(gb, &apu->channel1);
                channel_sweep(gb, &apu->channel1);
                channel_length(gb, &apu->channel2);
                channel_length(gb, &apu->channel3);
                channel_length(gb, &apu->channel4);
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

        if (gb->apu_speed > 2.0f || !gb->on_new_sample)  // don't collect samples when emulation speed increases too much
            return;

        apu->take_sample_cycles_count++;
        if (apu->take_sample_cycles_count >= (GB_CPU_FREQ / apu->dynamic_sampling_rate) * gb->apu_speed) {
            apu->take_sample_cycles_count = 0;

            // TODO keep it between -1.0f and 1.0f (AND MAYBE USE DOUBLES TO SEE IF BETTER QUALITY)
            //      ----> check how sameboy does it
            float S01_volume = ((mmu->io_registers[NR50 - IO] & 0x07) + 1) / 8.0f; // keep it between 0.0f and 1.0f
            float S02_volume = (((mmu->io_registers[NR50 - IO] & 0x70) >> 4) + 1) / 8.0f; // keep it between 0.0f and 1.0f
            float S01_output = ((CHECK_BIT(mmu->io_registers[NR51 - IO], APU_CHANNEL_1) ? channel_dac(gb, &apu->channel1) : 0.0f)
                                + (CHECK_BIT(mmu->io_registers[NR51 - IO], APU_CHANNEL_2) ? channel_dac(gb, &apu->channel2) : 0.0f)
                                + (CHECK_BIT(mmu->io_registers[NR51 - IO], APU_CHANNEL_3) ? channel_dac(gb, &apu->channel3) : 0.0f)
                                + (CHECK_BIT(mmu->io_registers[NR51 - IO], APU_CHANNEL_4) ? channel_dac(gb, &apu->channel4) : 0.0f)) / 4.0f;
            float S02_output = ((CHECK_BIT(mmu->io_registers[NR51 - IO], APU_CHANNEL_1 + 4) ? channel_dac(gb, &apu->channel1) : 0.0f)
                                + (CHECK_BIT(mmu->io_registers[NR51 - IO], APU_CHANNEL_2 + 4) ? channel_dac(gb, &apu->channel2) : 0.0f)
                                + (CHECK_BIT(mmu->io_registers[NR51 - IO], APU_CHANNEL_3 + 4) ? channel_dac(gb, &apu->channel3) : 0.0f)
                                + (CHECK_BIT(mmu->io_registers[NR51 - IO], APU_CHANNEL_4 + 4) ? channel_dac(gb, &apu->channel4) : 0.0f)) / 4.0f;

            // apply channel volume and global volume to its output
            S01_output = S01_output * S01_volume * gb->apu_sound_level;
            S02_output = S02_output * S02_volume * gb->apu_sound_level;

            gb->on_new_sample((gb_apu_sample_t) {.l = S02_output * 32767, .r = S01_output * 32767}, &apu->dynamic_sampling_rate);
        }
    }
}

// TODO find a way to make speed unnecessary
void apu_init(gb_t *gb) {
    apu_t *apu = xcalloc(1, sizeof(apu_t));
    apu->dynamic_sampling_rate = gb->apu_sampling_rate;

    apu->channel1 = (gb_channel_t) {
        .NRx0 = &gb->mmu->io_registers[NR10 - IO],
        .NRx1 = &gb->mmu->io_registers[NR11 - IO],
        .NRx2 = &gb->mmu->io_registers[NR12 - IO],
        .NRx3 = &gb->mmu->io_registers[NR13 - IO],
        .NRx4 = &gb->mmu->io_registers[NR14 - IO],
        .id = APU_CHANNEL_1
    };
    apu->channel2 = (gb_channel_t) { 
        .NRx1 = &gb->mmu->io_registers[NR21 - IO],
        .NRx2 = &gb->mmu->io_registers[NR22 - IO],
        .NRx3 = &gb->mmu->io_registers[NR23 - IO],
        .NRx4 = &gb->mmu->io_registers[NR24 - IO],
        .id = APU_CHANNEL_2
    };
    apu->channel3 = (gb_channel_t) {
        .NRx0 = &gb->mmu->io_registers[NR30 - IO],
        .NRx1 = &gb->mmu->io_registers[NR31 - IO],
        .NRx2 = &gb->mmu->io_registers[NR32 - IO],
        .NRx3 = &gb->mmu->io_registers[NR33 - IO],
        .NRx4 = &gb->mmu->io_registers[NR34 - IO],
        .id = APU_CHANNEL_3
    };
    apu->channel4 = (gb_channel_t) { 
        .NRx1 = &gb->mmu->io_registers[NR41 - IO],
        .NRx2 = &gb->mmu->io_registers[NR42 - IO],
        .NRx3 = &gb->mmu->io_registers[NR43 - IO],
        .NRx4 = &gb->mmu->io_registers[NR44 - IO],
        .LFSR = 0,
        .id = APU_CHANNEL_4
    };

    gb->apu = apu;
}

void apu_quit(gb_t *gb) {
    free(gb->apu);
}
