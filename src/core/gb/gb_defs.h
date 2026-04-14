#pragma once

#define GB_SCREEN_WIDTH  160
#define GB_SCREEN_HEIGHT 144

#define GB_CPU_FREQ          0x400000            // 4.19 MHz
#define GB_FRAMES_PER_SECOND (262144.0 / 4389.0) // 59.73 Hz

// 4194304 cycles executed per second --> 4194304 / fps --> 4194304 / 59.73 == 70224 cycles per frame
#define GB_CPU_CYCLES_PER_FRAME (GB_CPU_FREQ / GB_FRAMES_PER_SECOND)
// 70224 cycles per frame --> 70224 / 4 == 17556 steps per frame
#define GB_CPU_STEPS_PER_FRAME (GB_CPU_CYCLES_PER_FRAME / 4)

#define GB_CAMERA_SENSOR_WIDTH  128
#define GB_CAMERA_SENSOR_HEIGHT 128
