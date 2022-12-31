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

emulator_options_t defaults_opts = {
    .mode = DMG,
    .palette = PPU_COLOR_PALETTE_ORIG,
    .apu_speed = 1.0f,
    .apu_sound_level = 1.0f,
    .apu_sample_count = GB_APU_DEFAULT_SAMPLE_COUNT,
    .apu_format = APU_FORMAT_F32,
    .on_apu_samples_ready = NULL,
    .on_new_frame = NULL
};

const char *mbc_names[] = {
    STRINGIFY(ROM_ONLY),
    STRINGIFY(MBC1),
    STRINGIFY(MBC1M),
    STRINGIFY(MBC2),
    STRINGIFY(MBC3),
    STRINGIFY(MBC30),
    STRINGIFY(MBC5),
    STRINGIFY(MBC6),
    STRINGIFY(MBC7),
    STRINGIFY(HuC1)
};

void emulator_step(emulator_t *emu) {
    // stop execution of the program while a VBLANK DMA is active
    if (!emu->mmu->hdma.lock_cpu)
        cpu_step(emu);
    mmu_step(emu);
    timer_step(emu);
    link_step(emu);
    ppu_step(emu);
    apu_step(emu);
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
    if (mmu->has_eram && mmu->eram_banks > 0) {
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

    size_t eram_len = 0x2000 * emu->mmu->eram_banks;

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
    // make savestate header
    savestate_header_t *savestate_header = xmalloc(sizeof(savestate_header_t));
    savestate_header->mode = emu->mode;
    memcpy(savestate_header->identifier, FORMAT_STRING, sizeof(savestate_header->identifier));
    memcpy(savestate_header->rom_title, emu->rom_title, sizeof(savestate_header->rom_title));

    // make savestate data
    size_t cpu_len;
    byte_t *cpu = cpu_serialize(emu, &cpu_len);
    size_t timer_len;
    byte_t *timer = timer_serialize(emu, &timer_len);
    size_t link_len;
    byte_t *link = link_serialize(emu, &link_len);
    size_t ppu_len;
    byte_t *ppu = ppu_serialize(emu, &ppu_len);
    size_t mmu_len;
    byte_t *mmu = mmu_serialize(emu, &mmu_len);

    // don't write each component length into the savestate as the only variable length is the mmu which is written
    // last and it's length can be computed using the eram_banks number and the mode (both in the header)
    size_t savestate_data_len = cpu_len + timer_len + link_len + ppu_len + mmu_len;
    byte_t *savestate_data = xmalloc(savestate_data_len);
    size_t offset = 0;
    memcpy(savestate_data, cpu, cpu_len);
    offset += cpu_len;
    memcpy(&savestate_data[offset], timer, timer_len);
    offset += timer_len;
    memcpy(&savestate_data[offset], link, link_len);
    offset += link_len;
    memcpy(&savestate_data[offset], ppu, ppu_len);
    offset += ppu_len;
    memcpy(&savestate_data[offset], mmu, mmu_len);

    free(cpu);
    free(timer);
    free(link);
    free(ppu);
    free(mmu);

    // compress savestate data if specified
    #ifdef __HAVE_ZLIB__
    if (compressed) {
        SET_BIT(savestate_header->mode, 7);
        int t = compressBound(savestate_data_len);
        byte_t *dest = xmalloc(t);
        uLongf dest_len;
        int ret = compress(dest, &dest_len, savestate_data, savestate_data_len);
        free(savestate_data);
        savestate_data_len = dest_len;
        savestate_data = dest;

        printf("get_savestate %zu (compress:%d) buf=%d\n", sizeof(savestate_header_t) + savestate_data_len, ret, t);
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
        eprintf("invalid savestate length (%zu)\n", length);
        return 0;
    }

    savestate_header_t *savestate_header = xmalloc(sizeof(savestate_header_t));
    memcpy(savestate_header, data, sizeof(savestate_header_t));

    if (strncmp(savestate_header->identifier, FORMAT_STRING, sizeof(FORMAT_STRING))) {
        eprintf("invalid format\n");
        free(savestate_header);
        return 0;
    }
    if (strncmp(savestate_header->rom_title, emu->rom_title, sizeof(savestate_header->rom_title))) {
        eprintf("rom title mismatch (expected: '%.16s'; got: '%.16s')\n", emu->rom_title, savestate_header->rom_title);
        free(savestate_header);
        return 0;
    }

    size_t savestate_data_len = length - sizeof(savestate_header_t);
    byte_t *savestate_data = xmalloc(savestate_data_len);
    memcpy(savestate_data, data + sizeof(savestate_header_t), savestate_data_len);

    size_t cpu_len = cpu_serialized_length(emu);
    size_t timer_len = timer_serialized_length(emu);
    size_t link_len = link_serialized_length(emu);
    size_t ppu_len = ppu_serialized_length(emu);
    size_t mmu_len = mmu_serialized_length(emu);
    size_t expected_len = cpu_len + timer_len + link_len + ppu_len + mmu_len;

    if (CHECK_BIT(savestate_header->mode, 7)) {
        #ifdef __HAVE_ZLIB__
        byte_t *dest = xmalloc(expected_len);
        uLongf dest_len;
        int ret = uncompress(dest, &dest_len, savestate_data, savestate_data_len);
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

    if (savestate_data_len != expected_len) {
        eprintf("invalid savestate data length (expected: %zu; got: %zu)\n", expected_len, savestate_data_len);
        free(savestate_header);
        free(savestate_data);
        return 0;
    }

    if ((savestate_header->mode & 0x03) != emu->mode)
        emulator_reset(emu, savestate_header->mode & 0x03);

    size_t offset = 0;
    cpu_unserialize(emu, savestate_data);
    offset += cpu_len;
    timer_unserialize(emu, &savestate_data[offset]);
    offset += timer_len;
    link_unserialize(emu, &savestate_data[offset]);
    offset += link_len;
    ppu_unserialize(emu, &savestate_data[offset]);
    offset += ppu_len;
    mmu_unserialize(emu, &savestate_data[offset]);

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
        CPU_REQUEST_INTERRUPT(emu, IRQ_JOYPAD);
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
    opts->apu_format = emu->apu_format;
    opts->on_new_frame = emu->on_new_frame;
    opts->palette = emu->ppu_color_palette;
}

void emulator_set_options(emulator_t *emu, emulator_options_t *opts) {
    if (!opts)
        opts = &defaults_opts;

    // allow changes of mode, apu_sample_count and apu_format only once (inside emulator_init())
    if (!emu->mode) {
        emu->mode = opts->mode >= DMG && opts->mode <= CGB ? opts->mode : defaults_opts.mode;
        emu->apu_sample_count = opts->apu_sample_count >= 128 ? opts->apu_sample_count : defaults_opts.apu_sample_count;
        emu->apu_format = opts->apu_format >= APU_FORMAT_F32 && opts->apu_format <= APU_FORMAT_U8 ? opts->apu_format : defaults_opts.apu_format;
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
