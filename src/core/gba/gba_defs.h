#pragma once

#include "../core.h"

#define GBA_SCREEN_WIDTH  240
#define GBA_SCREEN_HEIGHT 160

#define GBA_CPU_FREQ          0x1000000           // 16.78 MHz
#define GBA_FRAMES_PER_SECOND (262144.0 / 4389.0) // 59.73 Hz

// 16777216 cycles executed per second --> 16777216 / fps --> 4194304 / 59.72 == 70224 cycles per frame
#define GBA_CPU_CYCLES_PER_FRAME (GBA_CPU_FREQ / GBA_FRAMES_PER_SECOND)
// 70224 cycles per frame --> 70224 steps per frame
#define GBA_CPU_STEPS_PER_FRAME GBA_CPU_CYCLES_PER_FRAME
