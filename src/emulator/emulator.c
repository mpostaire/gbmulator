#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#ifdef __HAVE_ZLIB__
#include <zlib.h>
#endif

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
    byte_t mode; // DMG or CGB
} savestate_header_t;

typedef struct __attribute__((packed)) {
    byte_t eram_banks;
    cpu_t cpu;
    gbtimer_t timer;
    byte_t mmu[];
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

const char *mbc_names[] = {
    "ROM_ONLY",
    "MBC1",
    "MBC2",
    "MBC3",
    "MBC30",
    "MBC5",
    "MBC6",
    "MBC7",
    "HuC1"
};

void emulator_step(emulator_t *emu) {
    // TODO make timings accurate by forcing each cpu_step() to take 4 cycles: if it's not enough to finish an instruction,
    // the next cpu_step() will resume the previous instruction. This will makes the timer "hack" (increment within a loop and not an if)
    // obsolete while allowing accurate memory timings emulation.

    // each instruction is multiple steps where each memory access is one step in the instruction

    // stop execution of the program while a VBLANK DMA is active
    int cycles;
    if (emu->mmu->hdma.lock_cpu) // implies that the emulator is running in CGB mode
        cycles = 4;
    else
        cpu_step(emu);

    // TODO remove this
    if (emu->mmu->mem[SC] == 0x81) {
        printf("%c", emu->mmu->mem[SB]);
        emu->mmu->mem[SC] = 0x00;
    }

    // TODO remove second arg of all the *_step(emu, 4) functions below once the cpu is implemented
    mmu_step(emu, 4);
    timer_step(emu, 4);
    link_step(emu, 4);
    ppu_step(emu, 4);
    apu_step(emu, 4);
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

void emulator_print_status(emulator_t *emu) {
    printf("[%s] Playing %s\n", emu->mode == DMG ? "DMG" : "CGB", emu->rom_title);

    mmu_t *mmu = emu->mmu;

    char *ram_str = NULL;
    if (mmu->has_eram) {
        ram_str = xmalloc(18);
        snprintf(ram_str, 17, " + %d RAM banks", mmu->eram_banks);
    }

    printf("Cartridge using %s with %d ROM banks%s%s%s%s\n",
            mbc_names[mmu->mbc],
            mmu->rom_banks,
            ram_str ? ram_str : "",
            mmu->has_battery ? " + BATTERY" : "",
            mmu->has_rtc ? " + RTC" : "",
            mmu->has_rumble ? " + RUMBLE" : ""
    );
    free(ram_str);
}

void emulator_link_connect(emulator_t *emu, emulator_t *other_emu) {
    emu->link->other_emu = other_emu;
    other_emu->link->other_emu = emu;
}

void emulator_link_disconnect(emulator_t *emu) {
    emu->link->other_emu->link->other_emu = NULL;
    emu->link->other_emu = NULL;
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

    size_t rtc_value_len = 0;
    size_t rtc_timestamp_len = 0;
    char *rtc_value_str = NULL;
    char *rtc_timestamp_str = NULL;
    if (emu->mmu->has_rtc) {
        rtc_value_str = time_to_string(emu->mmu->rtc.value_in_seconds, &rtc_value_len);
        rtc_timestamp_str = time_to_string(emu->mmu->rtc.timestamp, &rtc_timestamp_len);
    }

    size_t eram_len = emu->mmu->has_battery ? 0x2000 * emu->mmu->eram_banks : 0;
    size_t total_len = eram_len + rtc_value_len + rtc_timestamp_len;
    if (save_length)
        *save_length = total_len;

    if (total_len == 0)
        return NULL;

    byte_t *save_data = xmalloc(total_len);

    if (eram_len > 0)
        memcpy(save_data, emu->mmu->eram, eram_len);

    if (rtc_value_len + rtc_timestamp_len > 0) {
        memcpy(&save_data[eram_len], rtc_value_str, rtc_value_len);
        memcpy(&save_data[eram_len + rtc_value_len], rtc_timestamp_str, rtc_timestamp_len);
    }

    if (rtc_value_str)
        free(rtc_value_str);
    if (rtc_timestamp_str)
        free(rtc_timestamp_str);

    return save_data;
}

int emulator_load_save(emulator_t *emu, byte_t *save_data, size_t save_length) {
    // don't load save if the cartridge has no battery or there is no rtc and no eram banks
    if (!emu->mmu->has_battery || (!emu->mmu->has_rtc && emu->mmu->eram_banks == 0))
        return 0;

    size_t eram_len = emu->mmu->has_battery ? 0x2000 * emu->mmu->eram_banks : 0;

    size_t rtc_value_len = 0;
    size_t rtc_timestamp_len = 0;
    if (emu->mmu->has_rtc) {
        rtc_value_len = strlen((char *) &save_data[eram_len]) + 1;
        rtc_timestamp_len = strlen((char *) &save_data[eram_len + rtc_value_len]) + 1;
    }

    size_t total_len = eram_len + rtc_value_len + rtc_timestamp_len;
    if (total_len != save_length || total_len == 0)
        return 0;

    if (eram_len > 0)
        memcpy(emu->mmu->eram, save_data, eram_len);

    if (rtc_value_len + rtc_timestamp_len > 0) {
        emu->mmu->rtc.value_in_seconds = string_to_time((char *) &save_data[eram_len]);
        emu->mmu->rtc.timestamp = string_to_time((char *) &save_data[eram_len + rtc_value_len]);
    }

    return 1;
}

byte_t *emulator_get_savestate(emulator_t *emu, size_t *length, byte_t compressed) {
    size_t eram_len = 0x2000 * emu->mmu->eram_banks;
    size_t wram_extra_len = 0;
    size_t vram_extra_len = 0;
    size_t cram_len = 0;
    size_t hdma_len = 0;
    size_t rtc_len = 0;
    if (emu->mode == CGB) {
        wram_extra_len = sizeof(emu->mmu->wram_extra);
        vram_extra_len = sizeof(emu->mmu->vram_extra);
        cram_len = sizeof(emu->mmu->cram_bg) + sizeof(emu->mmu->cram_obj);
        hdma_len = sizeof(emu->mmu->hdma);
        rtc_len = sizeof(emu->mmu->rtc);
    }
    size_t mmu_header_len = (sizeof(mmu_t) - offsetof(mmu_t, mbc)) - (sizeof(mmu_t) - offsetof(mmu_t, eram));
    size_t mmu_len = mmu_header_len + eram_len + wram_extra_len + vram_extra_len + cram_len + hdma_len + rtc_len;

    // make savestate header
    savestate_header_t *savestate_header = xmalloc(sizeof(savestate_header_t));
    savestate_header->mode = emu->mode;
    memcpy(savestate_header->identifier, FORMAT_STRING, sizeof(savestate_header->identifier));
    memcpy(savestate_header->rom_title, emu->rom_title, sizeof(savestate_header->rom_title));

    // make savestate data
    size_t savestate_data_len = sizeof(savestate_data_t) + mmu_len;
    savestate_data_t *savestate_data = xmalloc(savestate_data_len);

    savestate_data->eram_banks = emu->mmu->eram_banks;
    memcpy(&savestate_data->cpu, emu->cpu, sizeof(cpu_t));
    memcpy(&savestate_data->timer, emu->timer, sizeof(gbtimer_t));

    memcpy(savestate_data->mmu, &emu->mmu->mbc, mmu_header_len);

    size_t offset = mmu_header_len;
    memcpy(savestate_data->mmu + offset, &emu->mmu->eram, eram_len);

    if (emu->mode == CGB) {
        offset += eram_len;
        memcpy(savestate_data->mmu + offset, &emu->mmu->wram_extra, mmu_len - offset);
    }

    // compress savestate data if specified
    #ifdef __HAVE_ZLIB__
    if (compressed) {
        SET_BIT(savestate_header->mode, 7);
        size_t t = compressBound(savestate_data_len);
        savestate_data_t *dest = xmalloc(t);
        size_t dest_len;
        int ret = compress((byte_t *) dest, &dest_len, (byte_t *) savestate_data, savestate_data_len);
        free(savestate_data);
        savestate_data_len = dest_len;
        savestate_data = dest;

        printf("get_savestate %ld (compress:%d) buf=%ld\n", sizeof(savestate_header_t) + savestate_data_len, ret, t);
    }
    #endif

    // assemble savestate header and savestate data
    *length = sizeof(savestate_header_t) + savestate_data_len;
    byte_t *savestate = xmalloc(*length);
    memcpy(savestate, savestate_header, sizeof(savestate_header_t));
    memcpy(savestate + sizeof(savestate_header_t), savestate_data, savestate_data_len);

    free(savestate_header);
    free(savestate_data);

    return (byte_t *) savestate;
}

int emulator_load_savestate(emulator_t *emu, const byte_t *data, size_t length) {
    if (length <= sizeof(savestate_header_t)) {
        eprintf("invalid savestate length (%ld)\n", length);
        return 0;
    }

    savestate_header_t *savestate_header = xmalloc(sizeof(savestate_header_t));
    memcpy(savestate_header, data, sizeof(savestate_header_t));

    if (strncmp(savestate_header->identifier, FORMAT_STRING, sizeof(FORMAT_STRING))) {
        eprintf("invalid format\n");
        return 0;
    }
    if (strncmp(savestate_header->rom_title, emu->rom_title, sizeof(savestate_header->rom_title))) {
        eprintf("rom title mismatch (expected: [%.16s]; got: [%.16s])\n", emu->rom_title, savestate_header->rom_title);
        return 0;
    }

    size_t savestate_data_len = length - sizeof(savestate_header_t);
    savestate_data_t *savestate_data = xmalloc(savestate_data_len);
    memcpy(savestate_data, data + sizeof(savestate_header_t), savestate_data_len);

    if (CHECK_BIT(savestate_header->mode, 7)) {
        #ifdef __HAVE_ZLIB__
        size_t t = sizeof(savestate_data_t) + (sizeof(mmu_t) - offsetof(mmu_t, mbc)) + 1e5;
        savestate_data_t *dest = xmalloc(t);
        size_t dest_len;
        int ret = uncompress((byte_t *) dest, &dest_len, (byte_t *) savestate_data, savestate_data_len);
        printf("uncompress=%d\n", ret);
        free(savestate_data);
        savestate_data_len = dest_len;
        savestate_data = dest;
        #else
        eprintf("this binary isn't compiled with compressed savestates support\n");
        free(savestate_header);
        free(savestate_data);
        return 0;
        #endif
    }

    size_t eram_len = 0x2000 * savestate_data->eram_banks;
    size_t wram_extra_len = 0;
    size_t vram_extra_len = 0;
    size_t cram_len = 0;
    size_t hdma_len = 0;
    size_t rtc_len = 0;
    if ((savestate_header->mode & 0x03) == CGB) {
        wram_extra_len = sizeof(emu->mmu->wram_extra);
        vram_extra_len = sizeof(emu->mmu->vram_extra);
        cram_len = sizeof(emu->mmu->cram_bg) + sizeof(emu->mmu->cram_obj);
        hdma_len = sizeof(emu->mmu->hdma);
        rtc_len = sizeof(emu->mmu->rtc);
    }
    size_t mmu_header_len = (sizeof(mmu_t) - offsetof(mmu_t, mbc)) - (sizeof(mmu_t) - offsetof(mmu_t, eram));
    size_t mmu_len = mmu_header_len + eram_len + wram_extra_len + vram_extra_len + cram_len + hdma_len + rtc_len;

    if (savestate_data->eram_banks > 16 || savestate_data_len != sizeof(savestate_data_t) + mmu_len) {
        eprintf("invalid savestate data length\n");
        return 0;
    }

    if ((savestate_header->mode & 0x03) != emu->mode)
        emulator_reset(emu, savestate_header->mode & 0x03);

    memcpy(emu->cpu, &savestate_data->cpu, sizeof(cpu_t));
    memcpy(emu->timer, &savestate_data->timer, sizeof(gbtimer_t));

    memcpy(&emu->mmu->mbc, savestate_data->mmu, mmu_header_len);

    size_t offset = mmu_header_len;
    memcpy(&emu->mmu->eram, savestate_data->mmu + offset, eram_len);

    if (emu->mode == CGB) {
        offset += eram_len;
        memcpy(&emu->mmu->wram_extra, savestate_data->mmu + offset, mmu_len - offset);
    }

    free(savestate_header);
    free(savestate_data);

    // resets apu's internal state to prevent glitchy audio if resuming from state without sound playing from state with sound playing
    apu_quit(emu);
    apu_init(emu);

    return emu->mode;
}

byte_t emulator_get_joypad_state(emulator_t *emu) {
    return (emu->joypad->action & 0x0F) << 4 | (emu->joypad->direction & 0x0F);
}

void emulator_set_joypad_state(emulator_t *emu, byte_t state) {
    emu->joypad->action = (state >> 4) & 0x0F;
    emu->joypad->direction = state & 0x0F;

    if (!CHECK_BIT(emu->mmu->mem[P1], 4) || !CHECK_BIT(emu->mmu->mem[P1], 5))
        cpu_request_interrupt(emu, IRQ_JOYPAD);
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
