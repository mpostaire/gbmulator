#include <stdio.h>
#include <SDL2/SDL.h>

#include "types.h"
#include "apu.h"
#include "mem.h"
#include "cpu.h"

#define SAMPLE_COUNT 2048

const byte_t duty_cycles[4][8] = {
    { 1, 0, 0, 0, 0, 0, 0, 0 },
    { 1, 1, 0, 0, 0, 0, 0, 0 },
    { 1, 1, 1, 1, 0, 0, 0, 0 },
    { 1, 1, 1, 1, 1, 1, 0, 0 }
};

SDL_AudioDeviceID device;
int take_sample_cycles_count = 0;

int audio_buffer_id = 0;
float audio_buffer[SAMPLE_COUNT];

byte_t frame_sequencer = 0;
int frame_sequencer_cycles_count = 0;

enum channel_id {
    CHANNEL_1,
    CHANNEL_2,
    CHANNEL_3,
    CHANNEL_4
};

channel_t channel1 = {
    .duty_position = 0,
    .duty = 0,
    .freq_timer = 0,
    .length_timer = 0,
    .envelope_timer = 0,
    .envelope_volume = 0,
    .sweep_timer = 0,
    .sweep_freq = 0,
    .sweep_enabled = 0,
    .NRx0 = &mem[NR10],
    .NRx1 = &mem[NR11],
    .NRx2 = &mem[NR12],
    .NRx3 = &mem[NR13],
    .NRx4 = &mem[NR14],
    .id = CHANNEL_1
};

channel_t channel2 = { 
    .duty_position = 0,
    .duty = 0,
    .freq_timer = 0,
    .length_timer = 0,
    .envelope_timer = 0,
    .envelope_volume = 0,
    .NRx1 = &mem[NR21],
    .NRx2 = &mem[NR22],
    .NRx3 = &mem[NR23],
    .NRx4 = &mem[NR24],
    .id = CHANNEL_2
};

channel_t channel3 = {
    .wave_position = 0,
    .freq_timer = 0,
    .length_timer = 0,
    .NRx0 = &mem[NR30],
    .NRx1 = &mem[NR31],
    .NRx2 = &mem[NR32],
    .NRx3 = &mem[NR33],
    .NRx4 = &mem[NR34],
    .id = CHANNEL_3
};

channel_t channel4 = { 
    .freq_timer = 0,
    .length_timer = 0,
    .envelope_timer = 0,
    .envelope_volume = 0,
    .NRx1 = &mem[NR41],
    .NRx2 = &mem[NR42],
    .NRx3 = &mem[NR43],
    .NRx4 = &mem[NR44],
    .LFSR = 0,
    .id = CHANNEL_4
};

static void channel_step(channel_t *c) {
    c->freq_timer--;
    if (c->freq_timer <= 0) {
        if (c->id == CHANNEL_4) {
            byte_t divisor = *c->NRx3;
            c->freq_timer = divisor ? divisor << 4 : 8;
            c->freq_timer <<= (*c->NRx4 >> 4);

            byte_t xor_ret = (c->LFSR & 0x01) ^ ((c->LFSR & 0x02) >> 1);
            c->LFSR = (c->LFSR >> 1) | (xor_ret << 14);

            if ((*c->NRx3 >> 3) & 0x01) {
                RESET_BIT(c->LFSR, 6);
                c->LFSR |= xor_ret << 6;
            }
            return;
        }

        word_t freq = ((*c->NRx4 & 0x07) << 8) | *c->NRx3;
        if (c->id == CHANNEL_3) {
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

    c->length_timer--;
    if (c->length_timer <= 0)
        RESET_BIT(mem[NR52], c->id); // disable channel
}

static int calc_sweep_freq(channel_t *c) {
    int freq = c->sweep_freq >> (*c->NRx0 & 0x07);
    if ((*c->NRx0 & 0x08) >> 3)
        freq = c->sweep_freq - freq;
    else
        freq = c->sweep_freq + freq;
    
    if (freq > 2047)
        RESET_BIT(mem[NR52], c->id); // disable channel
    
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

    c->envelope_timer--;
    if (c->envelope_timer <= 0) {
        c->envelope_timer = *c->NRx2 & 0x07;
        byte_t direction = (*c->NRx2 & 0x08) >> 3;
        if (c->envelope_volume < 0x0F && direction)
            c->envelope_volume++;
        else if (c->envelope_volume > 0x00 && !direction)
            c->envelope_volume--;
    }
}

void apu_channel_trigger(channel_t *c) {
    SET_BIT(mem[NR52], c->id); // channel enabled
    if (c->length_timer <= 0)
        c->length_timer = c->id == CHANNEL_3 ? 256 : 64;

    // TODO find out if this should really be done
    // byte_t freq = ((mem[NR14] & 0x03) << 8) | mem[NR13];
    // channel1_freq_timer = ((2048 - freq) * 4);
    
    if (c->id != CHANNEL_3) {
        c->envelope_timer = *c->NRx2 & 0x07;
        c->envelope_volume = *c->NRx2 >> 4;
    }

    if (c->id == CHANNEL_1) {
        c->sweep_freq = ((*c->NRx4 & 0x07) << 8) | *c->NRx3;
        byte_t sweep_period = (*c->NRx0 & 0x70) >> 4;
        c->sweep_timer = sweep_period > 0 ? sweep_period : 8;
        byte_t sweep_shift = *c->NRx0 & 0x07;
        c->sweep_enabled = (sweep_period > 0) || (sweep_shift > 0);
    }

    if (c->id == CHANNEL_4)
        c->LFSR = 0x7FFF;
}

// TODO reduce samples_count as the emulation speed increases to avoid losing performances??
void apu_step(int cycles) {
    if (!CHECK_BIT(mem[NR52], 7))
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
        if (take_sample_cycles_count >= 95) { // 44100 Hz
            take_sample_cycles_count = 0;

            float channel1_output = 0;
            // if (CHECK_BIT(mem[NR52], CHANNEL_1))
                // channel1_output = (((channel1.duty ? 1.0f : -1.0f) * channel1.envelope_volume)) / 15.0f; // divide by 15.0 to make it between -1.0 and 1.0

            float channel2_output = 0;
            // if (CHECK_BIT(mem[NR52], CHANNEL_2))
            //     channel2_output = (((channel2.duty ? 1.0f : -1.0f) * channel2.envelope_volume)) / 15.0f; // divide by 15.0 to make it between -1.0 and 1.0

            float channel3_output = 0;
            // if (CHECK_BIT(mem[NR52], CHANNEL_3)) {
            //     byte_t sample = mem[WAVE_RAM + (channel3.wave_position / 2)];
            //     if (channel3.wave_position & 1) {
            //         sample >>= 4;
            //     }
            //     sample &= 0x0F;
            //     switch ((mem[NR32] >> 5) & 0x03) {
            //         case 0:
            //             sample >>= 4; // mute
            //             break;
            //         case 2:
            //             sample >>= 1; // 50% volume
            //             break;
            //         case 3:
            //             sample >>= 2; // 25% volume
            //             break;
            //     }
            //     // TODO channel3 is buggy (compare pokemon red title screen with bgb)
            //     channel3_output = sample / 15.0f;
            //     // channel3_output = (sample / 7.5f) - 1.0f; // divide by 7.5 then substract 1.0 to make it between -1.0 and 1.0
            //     printf("%d\n", sample);
            // }

            // float channel4_output = (((channel4.LFSR & 0x01) * channel4.envelope_volume) / 15.0f);
            // printf("%d * %d = %f\n", (channel4.LFSR & 0x01), channel4.envelope_volume, channel4_output);
            float channel4_output = 0;

            float S01_volume = (mem[NR50] & 0x07) / 7.0f; // keep it between 0.0f and 1.0f
            float S02_volume = ((mem[NR50] & 0x70) >> 4) / 7.0f; // keep it between 0.0f and 1.0f
            float S01_output = (((CHECK_BIT(mem[NR51], CHANNEL_1) ? channel1_output : 0.0f) + (CHECK_BIT(mem[NR51], CHANNEL_2) ? channel2_output : 0.0f) + (CHECK_BIT(mem[NR51], CHANNEL_3) ? channel3_output : 0.0f) + (CHECK_BIT(mem[NR51], CHANNEL_4) ? channel4_output : 0.0f)) / 4.0f) * S01_volume;
            float S02_output = (((CHECK_BIT(mem[NR51], CHANNEL_1 + 4) ? channel1_output : 0.0f) + (CHECK_BIT(mem[NR51], CHANNEL_2 + 4) ? channel2_output : 0.0f) + (CHECK_BIT(mem[NR51], CHANNEL_3 + 4) ? channel3_output : 0.0f) + (CHECK_BIT(mem[NR51], CHANNEL_4 + 4) ? channel4_output : 0.0f)) / 4.0f) * S02_volume;

            // S01 (right)
            audio_buffer[audio_buffer_id++] = S01_output;
            // S02 (left)
            audio_buffer[audio_buffer_id++] = S02_output;
        }

        if (audio_buffer_id >= SAMPLE_COUNT) {
            audio_buffer_id = 0;
            while (SDL_GetQueuedAudioSize(device) > SAMPLE_COUNT * sizeof(float))
                SDL_Delay(1);
            SDL_QueueAudio(device, &audio_buffer, sizeof(audio_buffer));
        }
	}

}

void apu_init(void) {
    SDL_AudioSpec AudioSettings = { 0 };
    AudioSettings.freq = 44100;
    AudioSettings.format = AUDIO_F32SYS;
    AudioSettings.channels = 2;
    AudioSettings.samples = SAMPLE_COUNT;
    AudioSettings.callback = NULL;
    device = SDL_OpenAudioDevice(NULL, 0, &AudioSettings, NULL, 0); // TODO maybe don't allow any change
    SDL_PauseAudioDevice(device, 0);
}
