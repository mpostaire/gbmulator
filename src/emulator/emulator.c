#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "emulator.h"
#include "mmu.h"
#include "cpu.h"
#include "ppu.h"
#include "timer.h"
#include "joypad.h"
#include "link.h"
#include "apu.h"

#define FORMAT_STRING EMULATOR_NAME"-sav"

typedef struct __attribute__((packed)) {
    char identifier[sizeof(FORMAT_STRING)];
    char rom_title[16];
} save_header_t;

typedef struct __attribute__((packed)) {
    save_header_t header;
    cpu_t cpu;
    byte_t mmu[sizeof(mmu_t) - offsetof(mmu_t, mem)];
    gbtimer_t timer;
} savestate_data_t;

emulator_options_t defaults_opts = {
    .mode = DMG,
    .palette = PPU_COLOR_PALETTE_ORIG,
    .apu_speed = 1.0f,
    .apu_sound_level = 1.0f,
    .apu_sample_count = GB_APU_DEFAULT_SAMPLE_COUNT,
    .on_apu_samples_ready = NULL,
    .on_new_frame = NULL
};

int emulator_step(emulator_t *emu) {
    // TODO make timings accurate by forcing each cpu_step() to take 4 cycles: if it's not enough to finish an instruction,
    // the next cpu_step() will resume the previous instruction. This will makes the timer "hack" (increment within a loop and not an if)
    // obsolete while allowing accurate memory timings emulation.

    // each instruction is multiple steps where each memory access is one step in the instruction

    // stop execution of the program while a VBLANK DMA is active
    int cycles;
    if (emu->mmu->hdma.lock_cpu) // implies that the emulator is running in CGB mode
        cycles = 1;
    else
        cycles = cpu_step(emu);
    mmu_step(emu, cycles);
    timer_step(emu, cycles);
    link_step(emu, cycles);
    ppu_step(emu, cycles);
    apu_step(emu, cycles);
    return cycles;
}

emulator_t *emulator_init(const byte_t *rom_data, size_t rom_size, emulator_options_t *opts) {
    emulator_t *emu = xcalloc(1, sizeof(emulator_t));
    emulator_set_options(emu, opts);

    if (!mmu_init(emu, rom_data, rom_size)) {
        free(emu);
        return NULL;
    }
    cpu_init(emu);
    apu_init(emu);
    ppu_init(emu);
    timer_init(emu);
    link_init(emu);
    joypad_init(emu);

    return emu;
}

void emulator_quit(emulator_t *emu) {
    cpu_quit(emu);
    apu_quit(emu);
    mmu_quit(emu);
    ppu_quit(emu);
    timer_quit(emu);
    link_quit(emu);
    joypad_quit(emu);
    free(emu);
}


void emulator_reset(emulator_t *emu, emulator_mode_t mode) {
    size_t rom_size = emu->mmu->cartridge_size;
    byte_t *rom_data = xmalloc(rom_size);
    memcpy(rom_data, emu->mmu->cartridge, rom_size);

    emu->mode = 0;
    emulator_options_t opts;
    emulator_get_options(emu, &opts);
    opts.mode = mode;
    emulator_set_options(emu, &opts);

    size_t save_len;
    byte_t *save_data = emulator_get_save(emu, &save_len);

    cpu_quit(emu);
    apu_quit(emu);
    mmu_quit(emu);
    ppu_quit(emu);
    timer_quit(emu);
    link_quit(emu);
    joypad_quit(emu);

    mmu_init(emu, rom_data, rom_size);
    cpu_init(emu);
    apu_init(emu);
    ppu_init(emu);
    timer_init(emu);
    link_init(emu);
    joypad_init(emu);

    if (save_data)
        emulator_load_save(emu, save_data, save_len);

    free(rom_data);
}


int emulator_start_link(emulator_t *emu, const char *port, int is_ipv6, int mptcp_enabled) {
    return link_start_server(emu, port, is_ipv6, mptcp_enabled);
}

int emulator_connect_to_link(emulator_t *emu, const char *address, const char *port, int is_ipv6, int mptcp_enabled) {
    return link_connect_to_server(emu, address, port, is_ipv6, mptcp_enabled);
}

void emulator_joypad_press(emulator_t *emu, joypad_button_t key) {
    joypad_press(emu, key);
}

void emulator_joypad_release(emulator_t *emu, joypad_button_t key) {
    joypad_release(emu, key);
}

byte_t *emulator_get_save(emulator_t *emu, size_t *save_length) {
    if (!emu->mmu->has_battery || (!emu->mmu->has_rtc && emu->mmu->eram_banks == 0)) {
        *save_length = 0;
        return NULL;
    }

    size_t eram_len = emu->mmu->has_battery ? 0x2000 * emu->mmu->eram_banks : 0;
    size_t rtc_len = emu->mmu->has_rtc ? sizeof(rtc_t) - offsetof(rtc_t, value_in_seconds) : 0;
    size_t total_len = eram_len + rtc_len;
    if (save_length)
        *save_length = total_len;
    
    if (total_len == 0)
        return NULL;

    byte_t *save_data = xmalloc(total_len);

    if (eram_len > 0)
        memcpy(save_data, emu->mmu->eram, eram_len);

    if (rtc_len > 0)
        memcpy(&save_data[eram_len], &emu->mmu->rtc.value_in_seconds, rtc_len);

    return save_data;
}

int emulator_load_save(emulator_t *emu, byte_t *save_data, size_t save_length) {
    // don't load save if the cartridge has no battery or there is no rtc and no eram banks
    if (!emu->mmu->has_battery || (!emu->mmu->has_rtc && emu->mmu->eram_banks == 0))
        return 0;

    size_t eram_len = emu->mmu->has_battery ? 0x2000 * emu->mmu->eram_banks : 0;
    size_t rtc_len = emu->mmu->has_rtc ? sizeof(rtc_t) - offsetof(rtc_t, value_in_seconds) : 0;
    size_t total_len = eram_len + rtc_len;

    if (total_len != save_length || total_len == 0)
        return 0;

    if (eram_len > 0)
        memcpy(emu->mmu->eram, save_data, eram_len);

    if (rtc_len > 0)
        memcpy(&emu->mmu->rtc.value_in_seconds, &save_data[eram_len], rtc_len);

    return 1;
}

byte_t *emulator_get_savestate(emulator_t *emu, size_t *length) {
    savestate_data_t *savestate = xmalloc(sizeof(savestate_data_t));

    memcpy(savestate->header.identifier, FORMAT_STRING, sizeof(savestate->header.identifier));

    memcpy(savestate->header.rom_title, emu->rom_title, sizeof(savestate->header.rom_title));

    memcpy(&savestate->cpu, emu->cpu, sizeof(cpu_t));

    memcpy(&savestate->mmu, &emu->mmu->mem, sizeof(mmu_t) - offsetof(mmu_t, mem));

    memcpy(&savestate->timer, emu->timer, sizeof(gbtimer_t));

    *length = sizeof(savestate_data_t);
    return (byte_t *) savestate;
}

int emulator_load_savestate(emulator_t *emu, const byte_t *data, size_t length) {
    if (length != sizeof(savestate_data_t)) {
        eprintf("invalid data length\n");
        return 0;
    }

    savestate_data_t *savestate = (savestate_data_t *) data;

    if (strncmp(savestate->header.identifier, FORMAT_STRING, sizeof(FORMAT_STRING))) {
        eprintf("invalid format\n");
        return 0;
    }
    if (strncmp(savestate->header.rom_title, emu->rom_title, sizeof(savestate->header.rom_title))) {
        eprintf("rom title mismatch (expected: [%.16s]; got: [%.16s])\n", emu->rom_title, savestate->header.rom_title);
        return 0;
    }

    memcpy(emu->cpu, &savestate->cpu, sizeof(cpu_t));

    memcpy(&emu->mmu->mem, &savestate->mmu, sizeof(mmu_t) - offsetof(mmu_t, mem));

    memcpy(emu->timer, &savestate->timer, sizeof(gbtimer_t));

    // resets apu's internal state to prevent glitchy audio if resuming from state without sound playing from state with sound playing
    apu_quit(emu);
    apu_init(emu);

    return 1;
}

char *emulator_get_rom_title(emulator_t *emu) {
    return emu->rom_title;
}

byte_t *emulator_get_rom(emulator_t *emu, size_t *rom_size) {
    if (rom_size)
        *rom_size = emu->mmu->cartridge_size;
    return emu->mmu->cartridge;
}

char *emulator_get_rom_title_from_data(byte_t *rom_data, size_t size) {
    if (size < 0x144)
        return NULL;
    char *rom_title = xmalloc(17);
    memcpy(rom_title, (char *) &rom_data[0x134], 16);
    rom_title[16] = '\0';
    if (rom_data[0x0143] == 0xC0 || rom_data[0x0143] == 0x80)
        rom_title[15] = '\0';
    return rom_title;
}

void emulator_update_pixels_with_palette(emulator_t *emu, byte_t new_palette) {
    // replace old color values of the pixels with the new ones according to the new palette
    for (int i = 0; i < GB_SCREEN_WIDTH; i++) {
        for (int j = 0; j < GB_SCREEN_HEIGHT; j++) {
            byte_t *R = (emu->ppu->pixels + ((j) * GB_SCREEN_WIDTH * 3) + ((i) * 3)) ;
            byte_t *G = (emu->ppu->pixels + ((j) * GB_SCREEN_WIDTH * 3) + ((i) * 3) + 1);
            byte_t *B = (emu->ppu->pixels + ((j) * GB_SCREEN_WIDTH * 3) + ((i) * 3) + 2);

            // find which color is at pixel (i,j)
            for (dmg_color_t c = DMG_WHITE; c <= DMG_BLACK; c++) {
                if (*R == ppu_color_palettes[emu->ppu_color_palette][c][0] &&
                    *G == ppu_color_palettes[emu->ppu_color_palette][c][1] &&
                    *B == ppu_color_palettes[emu->ppu_color_palette][c][2]) {

                    // replace old color value by the new one according to the new palette
                    *R = ppu_color_palettes[new_palette][c][0];
                    *G = ppu_color_palettes[new_palette][c][1];
                    *B = ppu_color_palettes[new_palette][c][2];
                    break;
                }
            }
        }
    }
}

void emulator_get_options(emulator_t *emu, emulator_options_t *opts) {
    opts->mode = emu->mode;
    opts->apu_sample_count = emu->apu_sample_count;
    opts->on_apu_samples_ready = emu->on_apu_samples_ready;
    opts->apu_speed = emu->apu_speed;
    opts->apu_sound_level = emu->apu_sound_level;
    opts->on_new_frame = emu->on_new_frame;
    opts->palette = emu->ppu_color_palette;
}

void emulator_set_options(emulator_t *emu, emulator_options_t *opts) {
    if (!opts)
        opts = &defaults_opts;

    // allow changes of mode and apu_sample_count only once (inside emulator_init())
    if (!emu->mode) {
        emu->mode = opts->mode >= DMG && opts->mode <= CGB ? opts->mode : defaults_opts.mode;
        emu->apu_sample_count = opts->apu_sample_count >= 128 ? opts->apu_sample_count : defaults_opts.apu_sample_count;
    }

    emu->on_apu_samples_ready = opts->on_apu_samples_ready;
    emu->apu_speed = opts->apu_speed < 1.0f ? defaults_opts.apu_speed : opts->apu_speed;
    emu->apu_sound_level = opts->apu_sound_level > 1.0f ? defaults_opts.apu_sound_level : opts->apu_sound_level;
    emu->on_new_frame = opts->on_new_frame;
    emu->ppu_color_palette = opts->palette >= 0 && opts->palette < PPU_COLOR_PALETTE_MAX ? opts->palette : defaults_opts.palette;
}

byte_t *emulator_get_color_values(emulator_t *emu, dmg_color_t color) {
    return ppu_color_palettes[emu->ppu_color_palette][color];
}

byte_t *emulator_get_color_values_from_palette(color_palette_t palette, dmg_color_t color) {
    return ppu_color_palettes[palette][color];
}

byte_t *emulator_get_pixels(emulator_t *emu) {
    return emu->ppu->pixels;
}

void emulator_set_apu_speed(emulator_t *emu, float speed) {
    emulator_options_t opts;
    emulator_get_options(emu, &opts);
    opts.apu_speed = speed;
    emulator_set_options(emu, &opts);
}

void emulator_set_apu_sound_level(emulator_t *emu, float level) {
    emulator_options_t opts;
    emulator_get_options(emu, &opts);
    opts.apu_sound_level = level;
    emulator_set_options(emu, &opts);
}

void emulator_set_palette(emulator_t *emu, color_palette_t palette) {
    emulator_options_t opts;
    emulator_get_options(emu, &opts);
    opts.palette = palette;
    emulator_set_options(emu, &opts);
}
