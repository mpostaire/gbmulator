#pragma once

#define GB_SCREEN_WIDTH  160
#define GB_SCREEN_HEIGHT 144

#define GB_CPU_FREQ          4194304
#define GB_FRAMES_PER_SECOND 60
// 4194304 cycles executed per second --> 4194304 / fps --> 4194304 / 60 == 69905 cycles per frame (the Game Boy runs at approximatively 60 fps)
#define GB_CPU_CYCLES_PER_FRAME (GB_CPU_FREQ / GB_FRAMES_PER_SECOND)
#define GB_CPU_STEPS_PER_FRAME  (GB_CPU_CYCLES_PER_FRAME / 4)

#define GB_CAMERA_SENSOR_WIDTH  128
#define GB_CAMERA_SENSOR_HEIGHT 128
