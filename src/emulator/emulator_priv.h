#pragma once

#include "emulator.h"
#include "cpu.h"
#include "mmu.h"
#include "ppu.h"
#include "apu.h"
#include "timer.h"
#include "joypad.h"
#include "link.h"

struct emulator_t {
    emulator_mode_t mode;
    byte_t disable_cgb_color_correction;
    byte_t exit_on_invalid_opcode;
    float apu_sound_level;
    float apu_speed;
    int apu_sample_count;
    apu_audio_format_t apu_format;
    byte_t dmg_palette;
    on_new_frame_t on_new_frame;
    on_apu_samples_ready_t on_apu_samples_ready;

    char rom_title[17];

    cpu_t *cpu;
    mmu_t *mmu;
    ppu_t *ppu;
    apu_t *apu;
    gbtimer_t *timer;
    joypad_t *joypad;
    link_t *link;
};
