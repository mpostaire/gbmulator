#include <stdlib.h>
#include <stddef.h>

#include "emulator.h"
#include "mmu.h"
#include "cpu.h"
#include "ppu.h"
#include "timer.h"
#include "joypad.h"
#include "link.h"
#include "apu.h"

int emulator_step(emulator_t *emu) {
    // TODO make timings accurate by forcing each cpu_step() to take 4 cycles: if it's not enough to finish an instruction,
    // the next cpu_step() will resume the previous instruction. This will makes the timer "hack" (increment within a loop and not an if)
    // obsolete while allowing accurate memory timings emulation.

    // stop execution of the program while a VBLANK DMA is active
    int cycles;
    if (emu->mmu->hdma.lock_cpu) // implies that the emulator is running in CGB mode
        cycles = 4;
    else
        cycles = cpu_step(emu);
    mmu_step(emu, cycles);
    timer_step(emu, cycles);
    link_step(emu, cycles);
    ppu_step(emu, cycles);
    apu_step(emu, cycles);
    return cycles;
}

void emulator_run_cycles(emulator_t *emu, int cycles_limit) {
    for (int cycles_count = 0; cycles_count < cycles_limit; cycles_count += emulator_step(emu));
}

emulator_t *emulator_init(emulator_mode_t mode, char *rom_path, void (*ppu_vblank_cb)(byte_t *pixels), void (*apu_samples_ready_cb)(float *audio_buffer, int audio_buffer_size)) {
    emulator_t *emu = xcalloc(1, sizeof(emulator_t));
    emu->mode = mode;

    if (!mmu_init(emu, rom_path))
        return NULL;
    cpu_init(emu);
    apu_init(emu, 1.0f, 1.0f, apu_samples_ready_cb);
    ppu_init(emu, ppu_vblank_cb);
    timer_init(emu);
    link_init(emu);
    joypad_init(emu);

    return emu;
}

emulator_t *emulator_init_from_data(emulator_mode_t mode, const byte_t *rom_data, size_t size, void (*ppu_vblank_cb)(byte_t *pixels), void (*apu_samples_ready_cb)(float *audio_buffer, int audio_buffer_size)) {
    emulator_t *emu = xcalloc(1, sizeof(emulator_t));
    emu->mode = mode;

    if (!mmu_init_from_data(emu, rom_data, size))
        return NULL;
    cpu_init(emu);
    apu_init(emu, 1.0f, 1.0f, apu_samples_ready_cb);
    ppu_init(emu, ppu_vblank_cb);
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

int emulator_start_link(emulator_t *emu, const int port) {
    return link_start_server(emu, port);
}

int emulator_connect_to_link(emulator_t *emu, const char* address, const int port) {
    return link_connect_to_server(emu, address, port);
}

void emulator_joypad_press(emulator_t *emu, joypad_button_t key) {
    joypad_press(emu, key);
}

void emulator_joypad_release(emulator_t *emu, joypad_button_t key) {
    joypad_release(emu, key);
}

int emulator_save(emulator_t *emu, const char *path) {
    // don't save if the cartridge has no battery or there is no rtc and no eram banks
    if (!emu->mmu->has_battery || (!emu->mmu->has_rtc && emu->mmu->eram_banks == 0))
        return 0;

    make_parent_dirs(path);

    FILE *f = fopen(path, "wb");
    if (!f) {
        errnoprintf("opening the save file");
        return 0;
    }

    if (emu->mmu->eram_banks > 0) {
        if (!fwrite(emu->mmu->eram, 0x2000 * emu->mmu->eram_banks, 1, f)) {
            eprintf("writing eram to save file\n");
            fclose(f);
            return 0;
        }
    }

    if (emu->mmu->has_rtc) {
        if (!fwrite(&emu->mmu->rtc.value_in_seconds, sizeof(rtc_t) - offsetof(rtc_t, value_in_seconds), 1, f)) {
            eprintf("writing rtc to save file\n");
            fclose(f);
            return 0;
        }
    }

    fclose(f);
    return 1;
}

void emulator_load_save(emulator_t *emu, const char *path) {
    // don't load save if the cartridge has no battery or there is no rtc and no eram banks
    if (!emu->mmu->has_battery || (!emu->mmu->has_rtc && emu->mmu->eram_banks == 0))
        return;

    // load save into ERAM
    FILE *f = fopen(path, "rb");
    // if there is a save file, load it into eram
    if (f) {
        // no fread checks because a missing/invalid save file is not an error

        if (emu->mmu->eram_banks > 0)
            fread(emu->mmu->eram, 0x2000 * emu->mmu->eram_banks, 1, f);

        if (emu->mmu->has_rtc) {
            fread(&emu->mmu->rtc.value_in_seconds, sizeof(rtc_t) - offsetof(rtc_t, value_in_seconds), 1, f);
            rtc_update(&emu->mmu->rtc);
        }

        fclose(f);
    }
}

byte_t *emulator_get_save_data(emulator_t *emu, size_t *save_length) {
    if (!emu->mmu->mbc) {
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

void emulator_load_save_data(emulator_t *emu, byte_t *save_data, size_t save_len) {
    size_t eram_len = emu->mmu->has_battery ? 0x2000 * emu->mmu->eram_banks : 0;
    size_t rtc_len = emu->mmu->has_rtc ? sizeof(rtc_t) - offsetof(rtc_t, value_in_seconds) : 0;
    size_t total_len = eram_len + rtc_len;

    if (total_len != save_len || total_len == 0)
        return;

    if (eram_len > 0)
        memcpy(emu->mmu->eram, save_data, eram_len);

    if (rtc_len > 0)
        memcpy(&emu->mmu->rtc.value_in_seconds, &save_data[eram_len], rtc_len);
}

char *emulator_get_rom_path(emulator_t *emu) {
    return emu->rom_filepath;
}

char *emulator_get_rom_title(emulator_t *emu) {
    return emu->rom_title;
}

byte_t *emulator_get_rom_data(emulator_t *emu, size_t *rom_size) {
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
                if (*R == ppu_color_palettes[emu->ppu->current_color_palette][c][0] &&
                    *G == ppu_color_palettes[emu->ppu->current_color_palette][c][1] &&
                    *B == ppu_color_palettes[emu->ppu->current_color_palette][c][2]) {

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

byte_t emulator_get_color_palette(emulator_t *emu) {
    return emu->ppu->current_color_palette;
}

void emulator_set_color_palette(emulator_t *emu, color_palette_t palette) {
    emu->ppu->current_color_palette = palette;
}

byte_t *emulator_get_color_values(emulator_t *emu, dmg_color_t color) {
    return ppu_color_palettes[emu->ppu->current_color_palette][color];
}

byte_t *emulator_get_color_values_from_palette(color_palette_t palette, dmg_color_t color) {
    return ppu_color_palettes[palette][color];
}

byte_t *emulator_get_pixels(emulator_t *emu) {
    return emu->ppu->pixels;
}

void emulator_set_apu_speed(emulator_t *emu, float speed) {
    emu->apu->speed = speed;
}

void emulator_set_apu_sound_level(emulator_t *emu, float level) {
    emu->apu->global_sound_level = CLAMP(level, 0.0f, 1.0f);
}
