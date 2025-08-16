#pragma once

#include "gba.h"
#include "cpu.h"
#include "bus.h"
#include "ppu.h"
#include "tmr.h"
#include "dma.h"
#include "../utils.h"

struct gba_t {
    char rom_title[13]; // title is max 12 chars

    gbmulator_new_frame_cb_t on_new_frame;
    gbmulator_new_sample_cb_t on_new_sample;
    gbmulator_accelerometer_request_cb_t on_accelerometer_request;
    gbmulator_camera_capture_image_cb_t on_camera_capture_image;

    // TODO these don't have to be pointers (same for gb_t)
    gba_cpu_t *cpu;
    gba_bus_t *bus;
    gba_ppu_t *ppu;
    gba_dma_t *dma;
    gba_tmr_t *tmr;
};
