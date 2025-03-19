#pragma once

#include <AL/al.h>

#include "../../core/core.h"

/**
 * @param sampling_rate If > 0, this sets the sampling rate of the audio. It is detected automatically if set to 0.
 */
ALboolean alrenderer_init(ALsizei sampling_rate);

ALint alrenderer_get_sampling_rate(void);

void alrenderer_clear_queue(void);

void alrenderer_quit(void);

void alrenderer_enable_dynamic_rate_control(ALboolean enabled);

void alrenderer_queue_sample(const gb_apu_sample_t sample, uint32_t *dynamic_sampling_rate);

void alrenderer_pause(void);

void alrenderer_play(void);
