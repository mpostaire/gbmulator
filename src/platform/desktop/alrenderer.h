#pragma once

#include <AL/al.h>

ALboolean alrenderer_init(ALsizei sampling_freq);

ALboolean alrenderer_queue_audio(const byte_t *samples, ALsizei size);

ALint alrenderer_get_queue_size(void);
