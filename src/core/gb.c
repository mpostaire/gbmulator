#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#ifdef __HAVE_ZLIB__
#include <zlib.h>
#endif

#include "gb_priv.h"

#define SAVESTATE_STRING EMULATOR_NAME"-sav"

typedef struct __attribute__((packed)) {
    char identifier[sizeof(SAVESTATE_STRING)];
    char rom_title[16];
    byte_t mode; // DMG or CGB
} savestate_header_t;

// TODO fix the weird options getter/setters and default values implementation
gb_options_t defaults_opts = {
    .mode = DMG,
    .disable_cgb_color_correction = 0,
    .palette = PPU_COLOR_PALETTE_ORIG,
    .apu_speed = 1.0f,
    .apu_sound_level = 1.0f,
    .apu_sample_count = GB_APU_DEFAULT_SAMPLE_COUNT,
    .apu_format = APU_FORMAT_F32,
    .on_apu_samples_ready = NULL,
    .on_new_frame = NULL,
    .on_accelerometer_request = NULL
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
    STRINGIFY(MMM01),
    STRINGIFY(M161),
    STRINGIFY(HuC1),
    STRINGIFY(HuC3),
    STRINGIFY(CAMERA),
    STRINGIFY(TAMA5)
};

int gb_step(gb_t *gb) {
    byte_t double_speed = IS_DOUBLE_SPEED(gb);
    for (int i = double_speed + 1; i; i--) {
        // stop execution of the program while a GDMA or HDMA is active
        if (!gb->mmu->hdma.lock_cpu)
            cpu_step(gb);
        dma_step(gb);
        timer_step(gb);
        link_step(gb);
    }

    if (gb->mmu->has_rtc)
        rtc_step(gb);

    // TODO during the time the cpu is blocked after a STOP opcode triggering a speed switch, the ppu and apu
    //      behave in a weird way: https://gbdev.io/pandocs/CGB_Registers.html?highlight=key1#ff4d--key1-cgb-mode-only-prepare-speed-switch
    ppu_step(gb);
    apu_step(gb);

    return double_speed ? 8 : 4;
}

gb_t *gb_init(const byte_t *rom_data, size_t rom_size, gb_options_t *opts) {
    gb_t *gb = xcalloc(1, sizeof(gb_t));
    gb->exit_on_invalid_opcode = 1;
    gb_set_options(gb, opts);

    if (!mmu_init(gb, rom_data, rom_size)) {
        free(gb);
        return NULL;
    }
    cpu_init(gb);
    apu_init(gb);
    ppu_init(gb);
    timer_init(gb);
    link_init(gb);
    joypad_init(gb);

    return gb;
}

void gb_quit(gb_t *gb) {
    cpu_quit(gb);
    apu_quit(gb);
    mmu_quit(gb);
    ppu_quit(gb);
    timer_quit(gb);
    link_quit(gb);
    joypad_quit(gb);
    free(gb);
}

void gb_reset(gb_t *gb, gb_mode_t mode) {
    size_t rom_size = gb->mmu->rom_size;
    byte_t *rom_data = xmalloc(rom_size);
    memcpy(rom_data, gb->mmu->rom, rom_size);

    gb->mode = 0;
    gb_options_t opts;
    gb_get_options(gb, &opts);
    opts.mode = mode;
    gb_set_options(gb, &opts);

    size_t save_len;
    byte_t *save_data = gb_get_save(gb, &save_len);

    cpu_quit(gb);
    apu_quit(gb);
    mmu_quit(gb);
    ppu_quit(gb);
    timer_quit(gb);
    link_quit(gb);
    joypad_quit(gb);

    mmu_init(gb, rom_data, rom_size);
    cpu_init(gb);
    apu_init(gb);
    ppu_init(gb);
    timer_init(gb);
    link_init(gb);
    joypad_init(gb);

    if (save_data)
        gb_load_save(gb, save_data, save_len);

    free(rom_data);
}

static const char *get_new_licensee(gb_t *gb) {
    byte_t code = ((gb->mmu->rom[0x0144] - 0x30) * 10) + (gb->mmu->rom[0x0145]) - 0x30;

    switch (code) {
    case 0: return "None";
    case 1: return "Nintendo";
    case 8: return "Capcom";
    case 13: return "Electronic Arts";
    case 18: return "Hudson Soft";
    case 19: return "b-ai";
    case 20: return "kss";
    case 22: return "pow";
    case 24: return "PCM Complete";
    case 25: return "san-x";
    case 28: return "Kemco Japan";
    case 29: return "seta";
    case 30: return "Viacom";
    case 31: return "Nintendo";
    case 32: return "Bandai";
    case 33: return "Ocean/Acclaim";
    case 34: return "Konami";
    case 35: return "Hector";
    case 37: return "Taito";
    case 38: return "Hudson";
    case 39: return "Banpresto";
    case 41: return "Ubi Soft";
    case 42: return "Atlus";
    case 44: return "Malibu";
    case 46: return "angel";
    case 47: return "Bullet-Proof";
    case 49: return "irem";
    case 50: return "Absolute";
    case 51: return "Acclaim";
    case 52: return "Activision";
    case 53: return "American sammy";
    case 54: return "Konami";
    case 55: return "Hi tech entertainment";
    case 56: return "LJN";
    case 57: return "Matchbox";
    case 58: return "Mattel";
    case 59: return "Milton Bradley";
    case 60: return "Titus";
    case 61: return "Virgin";
    case 64: return "LucasArts";
    case 67: return "Ocean";
    case 69: return "Electronic Arts";
    case 70: return "Infogrames";
    case 71: return "Interplay";
    case 72: return "Broderbund";
    case 73: return "sculptured";
    case 75: return "sci";
    case 78: return "THQ";
    case 79: return "Accolade";
    case 80: return "misawa";
    case 83: return "lozc";
    case 86: return "Tokuma Shoten Intermedia";
    case 87: return "Tsukuda Original";
    case 91: return "Chunsoft";
    case 92: return "Video system";
    case 93: return "Ocean/Acclaim";
    case 95: return "Varie";
    case 96: return "Yonezawa/s’pal";
    case 97: return "Kaneko";
    case 99: return "Pack in soft";
    case 0xA4: return "Konami (Yu-Gi-Oh!)";
    default: return "Unknown";
    }
}

static const char *get_licensee(gb_t *gb) {
    switch (gb->mmu->rom[0x014B]) {
    case 0x00: return "None";
    case 0x01: return "Nintendo";
    case 0x08: return "Capcom";
    case 0x09: return "Hot-B";
    case 0x0A: return "Jaleco";
    case 0x0B: return "Coconuts Japan";
    case 0x0C: return "Elite Systems";
    case 0x13: return "EA (Electronic Arts)";
    case 0x18: return "Hudsonsoft";
    case 0x19: return "ITC Entertainment";
    case 0x1A: return "Yanoman";
    case 0x1D: return "Japan Clary";
    case 0x1F: return "Virgin Interactive";
    case 0x24: return "PCM Complete";
    case 0x25: return "San-X";
    case 0x28: return "Kotobuki Systems";
    case 0x29: return "Seta";
    case 0x30: return "Infogrames";
    case 0x31: return "Nintendo";
    case 0x32: return "Bandai";
    case 0x33: return get_new_licensee(gb);
    case 0x34: return "Konami";
    case 0x35: return "HectorSoft";
    case 0x38: return "Capcom";
    case 0x39: return "Banpresto";
    case 0x3C: return ".Entertainment i";
    case 0x3E: return "Gremlin";
    case 0x41: return "Ubisoft";
    case 0x42: return "Atlus";
    case 0x44: return "Malibu";
    case 0x46: return "Angel";
    case 0x47: return "Spectrum Holoby";
    case 0x49: return "Irem";
    case 0x4A: return "Virgin Interactive";
    case 0x4D: return "Malibu";
    case 0x4F: return "U.S. Gold";
    case 0x50: return "Absolute";
    case 0x51: return "Acclaim";
    case 0x52: return "Activision";
    case 0x53: return "American Sammy";
    case 0x54: return "GameTek";
    case 0x55: return "Park Place";
    case 0x56: return "LJN";
    case 0x57: return "Matchbox";
    case 0x59: return "Milton Bradley";
    case 0x5A: return "Mindscape";
    case 0x5B: return "Romstar";
    case 0x5C: return "Naxat Soft";
    case 0x5D: return "Tradewest";
    case 0x60: return "Titus";
    case 0x61: return "Virgin Interactive";
    case 0x67: return "Ocean Interactive";
    case 0x69: return "EA (Electronic Arts)";
    case 0x6E: return "Elite Systems";
    case 0x6F: return "Electro Brain";
    case 0x70: return "Infogrames";
    case 0x71: return "Interplay";
    case 0x72: return "Broderbund";
    case 0x73: return "Sculptered Soft";
    case 0x75: return "The Sales Curve";
    case 0x78: return "t.hq";
    case 0x79: return "Accolade";
    case 0x7A: return "Triffix Entertainment";
    case 0x7C: return "Microprose";
    case 0x7F: return "Kemco";
    case 0x80: return "Misawa Entertainment";
    case 0x83: return "Lozc";
    case 0x86: return "Tokuma Shoten Intermedia";
    case 0x8B: return "Bullet-Proof Software";
    case 0x8C: return "Vic Tokai";
    case 0x8E: return "Ape";
    case 0x8F: return "I’Max";
    case 0x91: return "Chunsoft Co.";
    case 0x92: return "Video System";
    case 0x93: return "Tsubaraya Productions Co.";
    case 0x95: return "Varie Corporation";
    case 0x96: return "Yonezawa/S’Pal";
    case 0x97: return "Kaneko";
    case 0x99: return "Arc";
    case 0x9A: return "Nihon Bussan";
    case 0x9B: return "Tecmo";
    case 0x9C: return "Imagineer";
    case 0x9D: return "Banpresto";
    case 0x9F: return "Nova";
    case 0xA1: return "Hori Electric";
    case 0xA2: return "Bandai";
    case 0xA4: return "Konami";
    case 0xA6: return "Kawada";
    case 0xA7: return "Takara";
    case 0xA9: return "Technos Japan";
    case 0xAA: return "Broderbund";
    case 0xAC: return "Toei Animation";
    case 0xAD: return "Toho";
    case 0xAF: return "Namco";
    case 0xB0: return "acclaim";
    case 0xB1: return "ASCII or Nexsoft";
    case 0xB2: return "Bandai";
    case 0xB4: return "Square Enix";
    case 0xB6: return "HAL Laboratory";
    case 0xB7: return "SNK";
    case 0xB9: return "Pony Canyon";
    case 0xBA: return "Culture Brain";
    case 0xBB: return "Sunsoft";
    case 0xBD: return "Sony Imagesoft";
    case 0xBF: return "Sammy";
    case 0xC0: return "Taito";
    case 0xC2: return "Kemco";
    case 0xC3: return "Squaresoft";
    case 0xC4: return "Tokuma Shoten Intermedia";
    case 0xC5: return "Data East";
    case 0xC6: return "Tonkinhouse";
    case 0xC8: return "Koei";
    case 0xC9: return "UFL";
    case 0xCA: return "Ultra";
    case 0xCB: return "Vap";
    case 0xCC: return "Use Corporation";
    case 0xCD: return "Meldac";
    case 0xCE: return ".Pony Canyon or";
    case 0xCF: return "Angel";
    case 0xD0: return "Taito";
    case 0xD1: return "Sofel";
    case 0xD2: return "Quest";
    case 0xD3: return "Sigma Enterprises";
    case 0xD4: return "ASK Kodansha Co.";
    case 0xD6: return "Naxat Soft";
    case 0xD7: return "Copya System";
    case 0xD9: return "Banpresto";
    case 0xDA: return "Tomy";
    case 0xDB: return "LJN";
    case 0xDD: return "NCS";
    case 0xDE: return "Human";
    case 0xDF: return "Altron";
    case 0xE0: return "Jaleco";
    case 0xE1: return "Towa Chiki";
    case 0xE2: return "Yutaka";
    case 0xE3: return "Varie";
    case 0xE5: return "Epcoh";
    case 0xE7: return "Athena";
    case 0xE8: return "Asmik ACE Entertainment";
    case 0xE9: return "Natsume";
    case 0xEA: return "King Records";
    case 0xEB: return "Atlus";
    case 0xEC: return "Epic/Sony Records";
    case 0xEE: return "IGS";
    case 0xF0: return "A Wave";
    case 0xF3: return "Extreme Entertainment";
    case 0xFF: return "LJN";
    default: return "Unknown";
    }
}

void gb_print_status(gb_t *gb) {
    gb_mmu_t *mmu = gb->mmu;

    printf("[%s] Playing %s (v%d) by %s\n",
            gb->mode == DMG ? "DMG" : "CGB",
            gb->rom_title,
            mmu->rom[0x014C],
            get_licensee(gb)
    );

    char *ram_str = NULL;
    if (mmu->eram_banks > 0) {
        ram_str = xmalloc(18);
        snprintf(ram_str, 17, " + %d RAM banks", mmu->eram_banks);
    }

    printf("Cartridge using %s with %d ROM banks%s%s%s%s\n",
            mbc_names[mmu->mbc.type],
            mmu->rom_banks,
            ram_str ? ram_str : "",
            mmu->has_battery ? " + BATTERY" : "",
            mmu->has_rtc ? " + RTC" : "",
            mmu->has_rumble ? " + RUMBLE" : ""
    );
    free(ram_str);
}

void gb_link_connect(gb_t *gb, gb_t *other_emu) {
    // disconnect any device that may have been connected before
    gb_link_disconnect(gb);
    gb_link_disconnect_printer(gb);

    gb->link->other_emu = other_emu;
    other_emu->link->other_emu = gb;
}

void gb_link_disconnect(gb_t *gb) {
    if (gb->link->other_emu)
        gb->link->other_emu->link->other_emu = NULL;
    gb->link->other_emu = NULL;
}

void gb_link_connect_printer(gb_t *gb, gb_printer_t *printer) {
    // disconnect any device that may have been connected before
    gb_link_disconnect(gb);
    gb_link_disconnect_printer(gb);

    gb->link->printer = printer;
}

void gb_link_disconnect_printer(gb_t *gb) {
    gb->link->printer = NULL;
}

void gb_joypad_press(gb_t *gb, gb_joypad_button_t key) {
    joypad_press(gb, key);
}

void gb_joypad_release(gb_t *gb, gb_joypad_button_t key) {
    joypad_release(gb, key);
}

byte_t *gb_get_save(gb_t *gb, size_t *save_length) {
    if (gb->mmu->mbc.type == MBC7) {
        *save_length = sizeof(gb->mmu->mbc.mbc7.eeprom.data);
        byte_t *save_data = xmalloc(*save_length);
        memcpy(save_data, gb->mmu->mbc.mbc7.eeprom.data, *save_length);
        return save_data;
    }

    if (!gb->mmu->has_battery || (!gb->mmu->has_rtc && gb->mmu->eram_banks == 0)) {
        *save_length = 0;
        return NULL;
    }

    size_t rtc_timestamp_len = 0;
    size_t rtc_len = 0;
    char *rtc_timestamp = NULL;
    if (gb->mmu->has_rtc) {
        rtc_timestamp = time_to_string(time(NULL), &rtc_timestamp_len);
        rtc_len = 10 + rtc_timestamp_len;
    }

    size_t eram_len = 0x2000 * gb->mmu->eram_banks;
    size_t total_len = eram_len + rtc_len;
    if (save_length)
        *save_length = total_len;

    if (total_len == 0)
        return NULL;

    byte_t *save_data = xmalloc(total_len);

    if (eram_len > 0)
        memcpy(save_data, gb->mmu->eram, eram_len);

    if (rtc_timestamp) {
        memcpy(&save_data[eram_len], &gb->mmu->mbc.mbc3.rtc.s, 1);
        memcpy(&save_data[eram_len + 1], &gb->mmu->mbc.mbc3.rtc.m, 1);
        memcpy(&save_data[eram_len + 2], &gb->mmu->mbc.mbc3.rtc.h, 1);
        memcpy(&save_data[eram_len + 3], &gb->mmu->mbc.mbc3.rtc.dl, 1);
        memcpy(&save_data[eram_len + 4], &gb->mmu->mbc.mbc3.rtc.dh, 1);
        memcpy(&save_data[eram_len + 5], &gb->mmu->mbc.mbc3.rtc.latched_s, 1);
        memcpy(&save_data[eram_len + 6], &gb->mmu->mbc.mbc3.rtc.latched_m, 1);
        memcpy(&save_data[eram_len + 7], &gb->mmu->mbc.mbc3.rtc.latched_h, 1);
        memcpy(&save_data[eram_len + 8], &gb->mmu->mbc.mbc3.rtc.latched_dl, 1);
        memcpy(&save_data[eram_len + 9], &gb->mmu->mbc.mbc3.rtc.latched_dh, 1);

        memcpy(&save_data[eram_len + 10], rtc_timestamp, rtc_timestamp_len);
        free(rtc_timestamp);
    }

    return save_data;
}

int gb_load_save(gb_t *gb, byte_t *save_data, size_t save_length) {
    if (gb->mmu->mbc.type == MBC7) {
        if (save_length != sizeof(gb->mmu->mbc.mbc7.eeprom.data))
            return 0;
        memcpy(gb->mmu->mbc.mbc7.eeprom.data, save_data, save_length);
        return 1;
    }

    // don't load save if the cartridge has no battery or there is no rtc and no eram banks
    if (!gb->mmu->has_battery || (!gb->mmu->has_rtc && gb->mmu->eram_banks == 0))
        return 0;

    size_t eram_len = gb->mmu->eram_banks * VRAM_BANK_SIZE;

    size_t rtc_len = 0;
    if (gb->mmu->has_rtc) {
        size_t rtc_timestamp_len = strlen((char *) &save_data[eram_len + 10]) + 1;
        rtc_len = 10 + rtc_timestamp_len;
    }

    size_t total_len = eram_len + rtc_len;
    if (total_len != save_length || total_len == 0)
        return 0;

    if (eram_len > 0)
        memcpy(gb->mmu->eram, save_data, eram_len);

    if (rtc_len > 0) {
        // get saved rtc registers and timestamp
        byte_t s = save_data[eram_len];
        byte_t m = save_data[eram_len + 1];
        byte_t h = save_data[eram_len + 2];
        byte_t dl = save_data[eram_len + 3];
        byte_t dh = save_data[eram_len + 4];
        gb->mmu->mbc.mbc3.rtc.latched_s = save_data[eram_len + 5];
        gb->mmu->mbc.mbc3.rtc.latched_m = save_data[eram_len + 6];
        gb->mmu->mbc.mbc3.rtc.latched_h = save_data[eram_len + 7];
        gb->mmu->mbc.mbc3.rtc.latched_dl = save_data[eram_len + 8];
        gb->mmu->mbc.mbc3.rtc.latched_dh = save_data[eram_len + 9];

        time_t rtc_timestamp = string_to_time((char *) &save_data[eram_len + 10]);

        // add elapsed time
        time_t rtc_registers_time = s + m * 60 + h * 3600 + ((dh << 8) | dl) * 86400;
        rtc_registers_time += time(NULL) - rtc_timestamp;

        // convert elapsed time back into rtc registers
        word_t d = rtc_registers_time / 86400;
        rtc_registers_time %= 86400;

        // day overflow
        if (d >= 0x0200) {
            SET_BIT(gb->mmu->mbc.mbc3.rtc.dh, 7);
            d %= 0x0200;
        }

        gb->mmu->mbc.mbc3.rtc.dh |= (d & 0x100) >> 8;
        gb->mmu->mbc.mbc3.rtc.dl = d & 0xFF;

        gb->mmu->mbc.mbc3.rtc.h = rtc_registers_time / 3600;
        rtc_registers_time %= 3600;

        gb->mmu->mbc.mbc3.rtc.m = rtc_registers_time / 60;
        rtc_registers_time %= 60;

        gb->mmu->mbc.mbc3.rtc.s = rtc_registers_time;
    }

    return 1;
}

#ifdef __HAVE_ZLIB__
byte_t *gb_get_savestate(gb_t *gb, size_t *length, byte_t compressed) {
#else
byte_t *gb_get_savestate(gb_t *gb, size_t *length, UNUSED byte_t compressed) {
#endif
    // make savestate header
    savestate_header_t *savestate_header = xmalloc(sizeof(savestate_header_t));
    savestate_header->mode = gb->mode;
    memcpy(savestate_header->identifier, SAVESTATE_STRING, sizeof(savestate_header->identifier));
    memcpy(savestate_header->rom_title, gb->rom_title, sizeof(savestate_header->rom_title));

    // make savestate data
    size_t cpu_len;
    byte_t *cpu = cpu_serialize(gb, &cpu_len);
    size_t timer_len;
    byte_t *timer = timer_serialize(gb, &timer_len);
    size_t link_len;
    byte_t *link = link_serialize(gb, &link_len);
    size_t ppu_len;
    byte_t *ppu = ppu_serialize(gb, &ppu_len);
    size_t mmu_len;
    byte_t *mmu = mmu_serialize(gb, &mmu_len);

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

int gb_load_savestate(gb_t *gb, const byte_t *data, size_t length) {
    if (length <= sizeof(savestate_header_t)) {
        eprintf("invalid savestate length (%zu)\n", length);
        return 0;
    }

    savestate_header_t *savestate_header = xmalloc(sizeof(savestate_header_t));
    memcpy(savestate_header, data, sizeof(savestate_header_t));

    if (strncmp(savestate_header->identifier, SAVESTATE_STRING, sizeof(SAVESTATE_STRING))) {
        eprintf("invalid format\n");
        free(savestate_header);
        return 0;
    }
    if (strncmp(savestate_header->rom_title, gb->rom_title, sizeof(savestate_header->rom_title))) {
        eprintf("rom title mismatch (expected: '%.16s'; got: '%.16s')\n", gb->rom_title, savestate_header->rom_title);
        free(savestate_header);
        return 0;
    }

    if ((savestate_header->mode & 0x03) != gb->mode)
        gb_reset(gb, savestate_header->mode & 0x03);

    size_t savestate_data_len = length - sizeof(savestate_header_t);
    byte_t *savestate_data = xmalloc(savestate_data_len);
    memcpy(savestate_data, data + sizeof(savestate_header_t), savestate_data_len);

    size_t cpu_len = cpu_serialized_length(gb);
    size_t timer_len = timer_serialized_length(gb);
    size_t link_len = link_serialized_length(gb);
    size_t ppu_len = ppu_serialized_length(gb);
    size_t mmu_len = mmu_serialized_length(gb);
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
        eprintf("this binary isn't compiled with support for compressed savestates\n");
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

    size_t offset = 0;
    cpu_unserialize(gb, savestate_data);
    offset += cpu_len;
    timer_unserialize(gb, &savestate_data[offset]);
    offset += timer_len;
    link_unserialize(gb, &savestate_data[offset]);
    offset += link_len;
    ppu_unserialize(gb, &savestate_data[offset]);
    offset += ppu_len;
    mmu_unserialize(gb, &savestate_data[offset]);

    free(savestate_header);
    free(savestate_data);

    // resets apu's internal state to prevent glitchy audio if resuming from state without sound playing from state with sound playing
    apu_quit(gb);
    apu_init(gb);

    return gb->mode;
}

byte_t gb_get_joypad_state(gb_t *gb) {
    return (gb->joypad->action & 0x0F) << 4 | (gb->joypad->direction & 0x0F);
}

void gb_set_joypad_state(gb_t *gb, byte_t state) {
    gb->joypad->action = (state >> 4) & 0x0F;
    gb->joypad->direction = state & 0x0F;

    if (!CHECK_BIT(gb->mmu->io_registers[P1 - IO], 4) || !CHECK_BIT(gb->mmu->io_registers[P1 - IO], 5))
        CPU_REQUEST_INTERRUPT(gb, IRQ_JOYPAD);
}

char *gb_get_rom_title(gb_t *gb) {
    return gb->rom_title;
}

byte_t *gb_get_rom(gb_t *gb, size_t *rom_size) {
    if (rom_size)
        *rom_size = gb->mmu->rom_size;
    return gb->mmu->rom;
}

char *gb_get_rom_title_from_data(byte_t *rom_data, size_t size) {
    if (size < 0x144)
        return NULL;
    char *rom_title = xmalloc(17);
    memcpy(rom_title, (char *) &rom_data[0x134], 16);
    rom_title[16] = '\0';
    if (rom_data[0x0143] == 0xC0 || rom_data[0x0143] == 0x80)
        rom_title[15] = '\0';
    return rom_title;
}

void gb_update_pixels_with_palette(gb_t *gb, byte_t new_palette) {
    // replace old color values of the pixels with the new ones according to the new palette
    for (int i = 0; i < GB_SCREEN_WIDTH; i++) {
        for (int j = 0; j < GB_SCREEN_HEIGHT; j++) {
            byte_t *R = (gb->ppu->pixels + ((j) * GB_SCREEN_WIDTH * 3) + ((i) * 3)) ;
            byte_t *G = (gb->ppu->pixels + ((j) * GB_SCREEN_WIDTH * 3) + ((i) * 3) + 1);
            byte_t *B = (gb->ppu->pixels + ((j) * GB_SCREEN_WIDTH * 3) + ((i) * 3) + 2);

            // find which color is at pixel (i,j)
            for (gb_dmg_color_t c = DMG_WHITE; c <= DMG_BLACK; c++) {
                if (*R == dmg_palettes[gb->dmg_palette][c][0] &&
                    *G == dmg_palettes[gb->dmg_palette][c][1] &&
                    *B == dmg_palettes[gb->dmg_palette][c][2]) {

                    // replace old color value by the new one according to the new palette
                    *R = dmg_palettes[new_palette][c][0];
                    *G = dmg_palettes[new_palette][c][1];
                    *B = dmg_palettes[new_palette][c][2];
                    break;
                }
            }
        }
    }
}

void gb_get_options(gb_t *gb, gb_options_t *opts) {
    opts->mode = gb->mode;
    opts->disable_cgb_color_correction = gb->disable_cgb_color_correction;
    opts->apu_sample_count = gb->apu_sample_count;
    opts->apu_speed = gb->apu_speed;
    opts->apu_sound_level = gb->apu_sound_level;
    opts->apu_format = gb->apu_format;
    opts->palette = gb->dmg_palette;
    opts->on_apu_samples_ready = gb->on_apu_samples_ready;
    opts->on_new_frame = gb->on_new_frame;
    opts->on_accelerometer_request = gb->on_accelerometer_request;
}

void gb_set_options(gb_t *gb, gb_options_t *opts) {
    if (!opts)
        opts = &defaults_opts;

    // allow changes of mode, apu_sample_count and apu_format only once (inside gb_init())
    if (!gb->mode) {
        gb->mode = opts->mode >= DMG && opts->mode <= CGB ? opts->mode : defaults_opts.mode;
        gb->apu_sample_count = opts->apu_sample_count >= 128 ? opts->apu_sample_count : defaults_opts.apu_sample_count;
        gb->apu_format = opts->apu_format >= APU_FORMAT_F32 && opts->apu_format <= APU_FORMAT_U8 ? opts->apu_format : defaults_opts.apu_format;
    }

    gb->disable_cgb_color_correction = opts->disable_cgb_color_correction;
    gb->apu_speed = opts->apu_speed < 1.0f ? defaults_opts.apu_speed : opts->apu_speed;
    gb->apu_sound_level = opts->apu_sound_level > 1.0f ? defaults_opts.apu_sound_level : opts->apu_sound_level;
    gb->dmg_palette = opts->palette >= 0 && opts->palette < PPU_COLOR_PALETTE_MAX ? opts->palette : defaults_opts.palette;
    gb->on_new_frame = opts->on_new_frame;
    gb->on_apu_samples_ready = opts->on_apu_samples_ready;
    gb->on_accelerometer_request = opts->on_accelerometer_request;
}

byte_t *gb_get_color_values(gb_t *gb, gb_dmg_color_t color) {
    return dmg_palettes[gb->dmg_palette][color];
}

byte_t *gb_get_color_values_from_palette(gb_color_palette_t palette, gb_dmg_color_t color) {
    return dmg_palettes[palette][color];
}

byte_t *gb_get_pixels(gb_t *gb) {
    return gb->ppu->pixels;
}

gb_mode_t gb_get_mode(gb_t *gb) {
    return gb->mode;
}

word_t gb_get_cartridge_checksum(gb_t *gb) {
    word_t checksum = 0;
    for (unsigned int i = 0; i < sizeof(gb->mmu->rom); i += 2)
        checksum = checksum - (gb->mmu->rom[i] + gb->mmu->rom[i + 1]) - 1;
    return checksum;
}

void gb_set_apu_speed(gb_t *gb, float speed) {
    gb_options_t opts;
    gb_get_options(gb, &opts);
    opts.apu_speed = speed;
    gb_set_options(gb, &opts);
}

void gb_set_apu_sound_level(gb_t *gb, float level) {
    gb_options_t opts;
    gb_get_options(gb, &opts);
    opts.apu_sound_level = level;
    gb_set_options(gb, &opts);
}

void gb_set_palette(gb_t *gb, gb_color_palette_t palette) {
    gb_options_t opts;
    gb_get_options(gb, &opts);
    opts.palette = palette;
    gb_set_options(gb, &opts);
}

byte_t gb_has_accelerometer(gb_t *gb) {
    return gb->mmu->mbc.type == MBC7;
}
