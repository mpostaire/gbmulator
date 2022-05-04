#include "utils.h"
#include "types.h"
#include "apu.h"
#include "mmu.h"
#include "cpu.h"

// FIXME rare audible pops

const byte_t duty_cycles[4][8] = {
    { 0, 0, 0, 0, 0, 0, 0, 1 },
    { 1, 0, 0, 0, 0, 0, 0, 1 },
    { 1, 0, 0, 0, 0, 1, 1, 1 },
    { 0, 1, 1, 1, 1, 1, 1, 0 }
};

float global_sound_level = 1.0f;

int take_sample_cycles_count = 0;
float sampling_speed_multiplier = 1.0f;
void (*samples_ready_cb)(float *audio_buffer);

int audio_buffer_index = 0;
float audio_buffer[APU_SAMPLE_COUNT];

byte_t frame_sequencer = 0;
int frame_sequencer_cycles_count = 0;

channel_t channel1;
channel_t channel2;
channel_t channel3;
channel_t channel4;

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

static void channel_length(channel_t *c) {
    if (!CHECK_BIT(*c->NRx4, 6)) // length enabled ?
        return;

    c->length_counter--;
    if (c->length_counter <= 0)
        APU_DISABLE_CHANNEL(c->id);
}

static int calc_sweep_freq(channel_t *c) {
    int freq = c->sweep_freq >> (*c->NRx0 & 0x07);
    if ((*c->NRx0 & 0x08) >> 3)
        freq = c->sweep_freq - freq;
    else
        freq = c->sweep_freq + freq;
    
    if (freq > 2047)
        APU_DISABLE_CHANNEL(c->id);
    
    return freq;
}

static void channel_sweep(channel_t *c) {
    c->sweep_timer--;
    if (c->sweep_timer <= 0) {
        byte_t sweep_period = (*c->NRx0 & 0x70) >> 4;
        c->sweep_timer = sweep_period > 0 ? sweep_period : 8;
    
        if (c->sweep_enabled && sweep_period > 0) {
            byte_t sweep_shift = *c->NRx0 & 0x07;
            int new_freq = calc_sweep_freq(c);
            
            if (new_freq < 2048 && sweep_shift > 0) {
                c->sweep_freq = new_freq;
                
                *c->NRx3 = new_freq & 0xFF;
                *c->NRx4 = (new_freq >> 8) & 0x07;

                calc_sweep_freq(c); // for the overflow check
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
void apu_channel_trigger(channel_t *c) {
    APU_ENABLE_CHANNEL(c->id);
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
            calc_sweep_freq(c);
    }

    if (c->id == APU_CHANNEL_4)
        c->LFSR = 0x7FFF;

    if ((c->id == APU_CHANNEL_3 && !CHECK_BIT(*c->NRx0, 7)) || (c->id != APU_CHANNEL_3 && !(*c->NRx2 >> 3)))
        APU_DISABLE_CHANNEL(c->id);
}

static float channel_dac(channel_t *c) {
    switch (c->id) {
    case APU_CHANNEL_1:
    case APU_CHANNEL_2:
        if ((*c->NRx2 >> 3) && APU_IS_CHANNEL_ENABLED(c->id)) // if dac enabled and channel enabled
            return ((c->duty * c->envelope_volume) / 7.5f) - 1.0f;
        break;
    case APU_CHANNEL_3:
        if ((*c->NRx0 >> 7) /*&& APU_IS_CHANNEL_ENABLED(c->id)*/) { // if dac enabled and channel enabled -- TODO check why channel 3 enabled flag is not working properly
            byte_t sample = mem[WAVE_RAM + (c->wave_position / 2)];
            if (c->wave_position % 2 == 0) // TODO check if this works properly (I think it always reads the 2 nibbles as the same values)
                sample >>= 4;
            sample &= 0x0F;
            switch ((mem[NR32] >> 5) & 0x03) {
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
        if ((*c->NRx2 >> 3) && APU_IS_CHANNEL_ENABLED(c->id)) // if dac enabled and channel enabled
            return ((!(c->LFSR & 0x01) * c->envelope_volume) / 7.5f) - 1.0f;
        break;
    }
    return 0.0f;
}

void apu_step(int cycles) {
    if (!IS_APU_ENABLED)
        return;

    while (cycles-- > 0) {
        frame_sequencer_cycles_count++;
        if (frame_sequencer_cycles_count >= 8192) { // 512 Hz
            frame_sequencer_cycles_count = 0;
            switch (frame_sequencer) {
            case 0:
                channel_length(&channel1);
                channel_length(&channel2);
                channel_length(&channel3);
                channel_length(&channel4);
                break;
            case 2:
                channel_length(&channel1);
                channel_sweep(&channel1);
                channel_length(&channel2);
                channel_length(&channel3);
                channel_length(&channel4);
                break;
            case 4:
                channel_length(&channel1);
                channel_length(&channel2);
                channel_length(&channel3);
                channel_length(&channel4);
                break;
            case 6:
                channel_length(&channel1);
                channel_sweep(&channel1);
                channel_length(&channel2);
                channel_length(&channel3);
                channel_length(&channel4);
                break;
            case 7:
                channel_envelope(&channel1);
                channel_envelope(&channel2);
                channel_envelope(&channel4);
                break;
            }
            frame_sequencer = (frame_sequencer + 1) % 8;
        }

        channel_step(&channel1);
        channel_step(&channel2);
        channel_step(&channel3);
        channel_step(&channel4);

        take_sample_cycles_count++;
        if (take_sample_cycles_count >= (CPU_FREQ / APU_SAMPLE_RATE) * sampling_speed_multiplier) { // 44100 Hz (if speed == 1.0f)
            take_sample_cycles_count = 0;

            float S01_volume = ((mem[NR50] & 0x07) + 1) / 8.0f; // keep it between 0.0f and 1.0f
            float S02_volume = (((mem[NR50] & 0x70) >> 4) + 1) / 8.0f; // keep it between 0.0f and 1.0f
            float S01_output = ((CHECK_BIT(mem[NR51], APU_CHANNEL_1) ? channel_dac(&channel1) : 0.0f) + (CHECK_BIT(mem[NR51], APU_CHANNEL_2) ? channel_dac(&channel2) : 0.0f) + (CHECK_BIT(mem[NR51], APU_CHANNEL_3) ? channel_dac(&channel3) : 0.0f) + (CHECK_BIT(mem[NR51], APU_CHANNEL_4) ? channel_dac(&channel4) : 0.0f)) / 4.0f;
            float S02_output = ((CHECK_BIT(mem[NR51], APU_CHANNEL_1 + 4) ? channel_dac(&channel1) : 0.0f) + (CHECK_BIT(mem[NR51], APU_CHANNEL_2 + 4) ? channel_dac(&channel2) : 0.0f) + (CHECK_BIT(mem[NR51], APU_CHANNEL_3 + 4) ? channel_dac(&channel3) : 0.0f) + (CHECK_BIT(mem[NR51], APU_CHANNEL_4 + 4) ? channel_dac(&channel4) : 0.0f)) / 4.0f;

            // S02 (left)
            audio_buffer[audio_buffer_index++] = S02_output * S02_volume * global_sound_level;
            // S01 (right)
            audio_buffer[audio_buffer_index++] = S01_output * S01_volume * global_sound_level;
        }

        if (audio_buffer_index >= APU_SAMPLE_COUNT) {
            audio_buffer_index = 0;
            samples_ready_cb(audio_buffer);
        }
    }

}

void apu_set_sampling_speed_multiplier(float speed) {
    sampling_speed_multiplier = speed;
}

void apu_set_global_sound_level(float sound) {
    if (sound >= 0.0f && sound <= 1.0f)
        global_sound_level = sound;
}

float apu_get_global_sound_level(void) {
    return global_sound_level;
}

void apu_set_samples_ready_callback(void (*samples_ready_callback)(float *audio_buffer)) {
    samples_ready_cb = samples_ready_callback;
}

void apu_init(void) {
    take_sample_cycles_count = 0;

    audio_buffer_index = 0;
    memset(audio_buffer, 0, sizeof(audio_buffer));

    frame_sequencer = 0;
    frame_sequencer_cycles_count = 0;

    channel1 = (channel_t) {
        .NRx0 = &mem[NR10],
        .NRx1 = &mem[NR11],
        .NRx2 = &mem[NR12],
        .NRx3 = &mem[NR13],
        .NRx4 = &mem[NR14],
        .id = APU_CHANNEL_1
    };

    channel2 = (channel_t) { 
        .NRx1 = &mem[NR21],
        .NRx2 = &mem[NR22],
        .NRx3 = &mem[NR23],
        .NRx4 = &mem[NR24],
        .id = APU_CHANNEL_2
    };

    channel3 = (channel_t) {
        .NRx0 = &mem[NR30],
        .NRx1 = &mem[NR31],
        .NRx2 = &mem[NR32],
        .NRx3 = &mem[NR33],
        .NRx4 = &mem[NR34],
        .id = APU_CHANNEL_3
    };

    channel4 = (channel_t) { 
        .NRx1 = &mem[NR41],
        .NRx2 = &mem[NR42],
        .NRx3 = &mem[NR43],
        .NRx4 = &mem[NR44],
        .LFSR = 0,
        .id = APU_CHANNEL_4
    };
}
