#include <stdio.h>
#include <AL/al.h>
#include <AL/alc.h>

#include "../../core/core.h"

#define AL_ERROR() alGetError() != AL_NO_ERROR

#define N_BUFFERS 8
#define N_SAMPLES 1024 // higher value reduces pitch variations, especially after init_buffers (best: 1024, min where is still good enough: 512)

#define DRC_TARGET_QUEUE_SIZE ((N_BUFFERS / 4) * N_SAMPLES)
#define DRC_MAX_FREQ_DIFF     0.02
#define DRC_ALPHA             0.1

typedef struct {
    ALCdevice             *device;
    ALCcontext            *context;
    ALuint                 source;
    ALint                  sampling_rate;
    ALuint                 buffers[N_BUFFERS];
    gbmulator_apu_sample_t samples[N_SAMPLES];
    ALsizei                samples_count;
    float                  sound_level;
    ALboolean              drc_enabled;
    int                    ewma_queue_size;
} alrenderer_t;

static alrenderer_t alr;

static inline void init_buffers(void) {
    // fill buffers with empty data to tell the source what format and sampling_rate they are using
    // (if we don't do this, queueing other data will fail until the queue has been emptied and we don't want this)
    alGenBuffers(N_BUFFERS, alr.buffers);
    for (int i = 0; i < N_BUFFERS; i++)
        alBufferData(alr.buffers[i], AL_FORMAT_STEREO16, NULL, 0, alr.sampling_rate);

    // queue and play all the empty buffers (they will be marked as processed immediately but it simplifies the audio queueing logic)
    alSourceQueueBuffers(alr.source, N_BUFFERS, alr.buffers);
    alSourcePlay(alr.source);

    alr.samples_count   = 0;
    alr.ewma_queue_size = DRC_TARGET_QUEUE_SIZE; // init at DRC_TARGET_QUEUE_SIZE to avoid high pitch variations at the beginning
}

ALboolean alrenderer_init(ALuint sampling_freq) {
    memset(&alr, 0, sizeof(alr));

    // Ouverture du device
    alr.device = alcOpenDevice(NULL);
    if (!alr.device) {
        eprintf("Error opening audio device\n");
        return AL_FALSE;
    }

    // Création du contexte
    alr.context = alcCreateContext(alr.device, NULL);
    if (!alr.context) {
        eprintf("Error creating audio context\n");
        return AL_FALSE;
    }

    // Activation du contexte
    if (!alcMakeContextCurrent(alr.context))
        return AL_FALSE;

    if (sampling_freq) {
        alr.sampling_rate = sampling_freq;
    } else {
        alcGetIntegerv(alr.device, ALC_FREQUENCY, 1, &alr.sampling_rate);
        if (AL_ERROR()) {
            eprintf("Error getting device sampling rate\n");
            return AL_FALSE;
        }
    }

    // printf("OpenAL version %s\n", alGetString(AL_VERSION));

    alGenSources(1, &alr.source);
    init_buffers();

    alr.drc_enabled = AL_TRUE;
    return AL_TRUE;
}

void alrenderer_clear_queue(void) {
    alSourceStop(alr.source);

    ALint processed;
    alGetSourcei(alr.source, AL_BUFFERS_PROCESSED, &processed);
    if (AL_ERROR())
        processed = 0;

    while (processed--) {
        ALuint buffer;
        alSourceUnqueueBuffers(alr.source, 1, &buffer);
        alDeleteBuffers(1, &buffer);
    }

    init_buffers();
}

void alrenderer_quit(void) {
    if (alr.source != 0) {
        alrenderer_clear_queue();

        alDeleteSources(1, &alr.source);
        alr.source = 0;
    }

    if (alr.context) {
        alcDestroyContext(alr.context);
        alr.context = NULL;
    }

    if (alr.device) {
        alcCloseDevice(alr.device);
        alr.device = NULL;
    }

    alr.sampling_rate = 0;
}

void alrenderer_pause(void) {
    alSourcePause(alr.source);
}

void alrenderer_play(void) {
    alSourcePlay(alr.source);
}

ALint alrenderer_get_sampling_rate(void) {
    return alr.sampling_rate;
}

static inline ALuint alrenderer_get_queue_size(void) {
    // Get the number of all attached buffers
    ALint queued;
    alGetSourcei(alr.source, AL_BUFFERS_QUEUED, &queued);
    if (AL_ERROR())
        queued = 0;

    // Get the number of all processed buffers (ready to be detached)
    ALint processed;
    alGetSourcei(alr.source, AL_BUFFERS_PROCESSED, &processed);
    if (AL_ERROR())
        processed = 0;

    return (queued - processed) * sizeof(alr.samples) + alr.samples_count * sizeof(*alr.samples);
}

void alrenderer_enable_dynamic_rate_control(ALboolean enabled) {
    alr.drc_enabled = enabled;
}

static inline uint32_t dynamic_rate_control(void) {
    // https://github.com/kevinbchen/nes-emu/blob/a993b0a5c080bc689de5f41e1e492e9e219e14e6/src/audio.cpp#L39
    int queue_size      = alrenderer_get_queue_size() / sizeof(gbmulator_apu_sample_t);
    alr.ewma_queue_size = queue_size * DRC_ALPHA + alr.ewma_queue_size * (1.0 - DRC_ALPHA);

    // Adjust sample frequency to try and maintain a constant queue size
    double diff        = (alr.ewma_queue_size - DRC_TARGET_QUEUE_SIZE) / (double) DRC_TARGET_QUEUE_SIZE;
    int    sample_rate = alr.sampling_rate * (1.0 - CLAMP(diff, -1.0, 1.0) * (DRC_MAX_FREQ_DIFF));

    // double fill_level = queue_size / (double) (N_SAMPLES * N_BUFFERS);
    // printf("fill_level=%lf dynamic_frequency=%d (sampling_rate=%d) DRC_TARGET_QUEUE_SIZE=%d queue_size=%d ewma_queue_size=%d\n",
    //         fill_level, (ALsizei) sample_rate, sampling_rate, DRC_TARGET_QUEUE_SIZE, queue_size, ewma_queue_size);

    return sample_rate;
}

void alrenderer_queue_sample(const gbmulator_apu_sample_t sample, uint32_t *dynamic_sampling_rate) {
    alr.samples[alr.samples_count++] = (gbmulator_apu_sample_t) { .l = sample.l * alr.sound_level, .r = sample.r * alr.sound_level };

    if (alr.samples_count < N_SAMPLES)
        return;
    alr.samples_count = 0;

    ALint processed;
    alGetSourcei(alr.source, AL_BUFFERS_PROCESSED, &processed);
    if (AL_ERROR()) {
        eprintf("Error checking source state\n");
        return;
    }

    if (processed <= 0)
        return; // drop these samples if the audio queue is full

    ALuint recycled_buffer;
    alSourceUnqueueBuffers(alr.source, 1, &recycled_buffer);
    if (AL_ERROR()) {
        eprintf("Error unqueueing buffer\n");
        return;
    }

    alBufferData(recycled_buffer, AL_FORMAT_STEREO16, alr.samples, sizeof(alr.samples), alr.sampling_rate);
    alSourceQueueBuffers(alr.source, 1, &recycled_buffer);
    if (AL_ERROR()) {
        eprintf("Error buffering data\n");
        return;
    }

    if (alr.drc_enabled)
        *dynamic_sampling_rate = dynamic_rate_control(); // TODO takes time to stabilize itself at the beginning and this is noticeable

    ALint state;
    alGetSourcei(alr.source, AL_SOURCE_STATE, &state);
    if (AL_ERROR()) {
        eprintf("Error getting source state\n");
        return;
    }

    // Make sure the source hasn't underrun
    if (state != AL_PLAYING && state != AL_PAUSED) {
        // If no buffers are queued, playback is finished
        ALint queued;
        alGetSourcei(alr.source, AL_BUFFERS_QUEUED, &queued);
        if (AL_ERROR()) {
            eprintf("Error getting queued buffers\n");
            return;
        } else if (queued == 0) {
            return;
        }

        alSourcePlay(alr.source);
        if (AL_ERROR()) {
            eprintf("Error restarting playback\n");
            return;
        }
    }
}

void alrenderer_set_level(float level) {
    alr.sound_level = CLAMP(level, 0.0f, 1.0f);
}
