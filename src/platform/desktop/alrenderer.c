#include <stdio.h>
#include <AL/al.h>
#include <AL/alc.h>

#include "../../emulator/emulator.h"

#define N_BUFFERS 16

ALuint buffers[N_BUFFERS];
ALuint source;
ALsizei freq;

ALboolean alrenderer_init(ALsizei sampling_freq) {
    // Ouverture du device
    ALCdevice *device = alcOpenDevice(NULL);
    if (!device) {
        eprintf("Error opening audio device\n");
        return AL_FALSE;
    }

    // Création du contexte
    ALCcontext *context = alcCreateContext(device, NULL);
    if (!context) {
        eprintf("Error creation audio context\n");
        return AL_FALSE;
    }

    // Activation du contexte
    if (!alcMakeContextCurrent(context))
        return AL_FALSE;

    freq = sampling_freq;

    // printf("OpenAL version %s\n", alGetString(AL_VERSION));

    alGenBuffers(N_BUFFERS, buffers);
    alGenSources(1, &source);

    // fill buffers with empty data to tell the source what format and freq it will be using
    // (if we don't do this, queueing other data will fail until the queue has been emptied and we don't want this)
    byte_t temp = 0;
    for (int i = 0; i < N_BUFFERS; i++)
        alBufferData(buffers[i], AL_FORMAT_STEREO8, &temp, 0, freq);

    // queue and play all the empty buffers (they will be marked as processed immediately but it simplifies the audio queueing logic)
    alSourceQueueBuffers(source, N_BUFFERS, buffers);
    alSourcePlay(source);

    return AL_TRUE;
}

// TODO maybe this does not do exactlty what I think
ALint alrenderer_get_queue_size(void) {
    ALint queued;
    alGetSourcei(source, AL_BUFFERS_QUEUED, &queued);
    return queued;
}

ALboolean alrenderer_queue_audio(const void *samples, ALsizei size) {
    ALint processed;
    alGetSourcei(source, AL_BUFFERS_PROCESSED, &processed);
    if (alGetError() != AL_NO_ERROR) {
        eprintf("Error checking source state\n");
        return AL_FALSE;
    }

    if (processed <= 0)
        return AL_FALSE;

    ALuint available_buffer;
    alSourceUnqueueBuffers(source, 1, &available_buffer);

    alBufferData(available_buffer, AL_FORMAT_STEREO8, samples, size, freq);
    alSourceQueueBuffers(source, 1, &available_buffer);
    if (alGetError() != AL_NO_ERROR) {
        eprintf("Error buffering data\n");
        return AL_FALSE;
    }

    ALint state;
    alGetSourcei(source, AL_SOURCE_STATE, &state);

    // Make sure the source hasn't underrun
    if (state != AL_PLAYING && state != AL_PAUSED) {
        // If no buffers are queued, playback is finished
        ALint queued;
        alGetSourcei(source, AL_BUFFERS_QUEUED, &queued);
        if (queued == 0)
            return AL_FALSE;

        alSourcePlay(source);
        if (alGetError() != AL_NO_ERROR) {
            eprintf("Error restarting playback\n");
            return AL_FALSE;
        }
    }

    return AL_TRUE;
}
