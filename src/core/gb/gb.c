#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <zlib.h>

#include "gb_priv.h"

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

void gb_step(gb_t *gb) {
    uint8_t double_speed = IS_DOUBLE_SPEED(gb);
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
    if (gb->mmu->mbc.type == CAMERA)
        camera_step(gb);

    // TODO during the time the cpu is blocked after a STOP opcode triggering a speed switch, the ppu and apu
    //      behave in a weird way: https://gbdev.io/pandocs/CGB_Registers.html?highlight=key1#ff4d--key1-cgb-mode-only-prepare-speed-switch
    ppu_step(gb);
    apu_step(gb);
}

int gb_is_rom_valid(const uint8_t *rom) {
    return parse_header_mbc_byte(rom[0x0147], NULL, NULL, NULL, NULL, NULL) && validate_header_checksum(rom);
}

gb_t *gb_init(gbmulator_t *base) {
    gb_t *gb = xcalloc(1, sizeof(*gb));
    gb->base = base;

    gb->cgb_mode_enabled = gb->base->opts.mode == GBMULATOR_MODE_GBC;

    gb_set_palette(gb, gb->base->opts.palette);

    if (!mmu_init(gb, base->opts.rom, base->opts.rom_size)) {
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

static const char *old_licensees[] = {
    [0x01] = "Nintendo", [0x08] = "Capcom", [0x09] = "Hot-B", [0x0A] = "Jaleco", [0x0B] = "Coconuts Japan", [0x0C] = "Elite Systems",
    [0x13] = "EA (Electronic Arts)", [0x18] = "Hudsonsoft", [0x19] = "ITC Entertainment", [0x1A] = "Yanoman", [0x1D] = "Japan Clary", [0x1F] = "Virgin Interactive",
    [0x24] = "PCM Complete", [0x25] = "San-X", [0x28] = "Kotobuki Systems", [0x29] = "Seta", [0x30] = "Infogrames", [0x31] = "Nintendo",
    [0x32] = "Bandai", [0x34] = "Konami", [0x35] = "HectorSoft", [0x38] = "Capcom", [0x39] = "Banpresto", [0x3C] = ".Entertainment i",
    [0x3E] = "Gremlin", [0x41] = "Ubisoft", [0x42] = "Atlus", [0x44] = "Malibu", [0x46] = "Angel", [0x47] = "Spectrum Holoby",
    [0x49] = "Irem", [0x4A] = "Virgin Interactive", [0x4D] = "Malibu", [0x4F] = "U.S. Gold", [0x50] = "Absolute", [0x51] = "Acclaim",
    [0x52] = "Activision", [0x53] = "American Sammy", [0x54] = "GameTek", [0x55] = "Park Place", [0x56] = "LJN", [0x57] = "Matchbox",
    [0x59] = "Milton Bradley", [0x5A] = "Mindscape", [0x5B] = "Romstar", [0x5C] = "Naxat Soft", [0x5D] = "Tradewest", [0x60] = "Titus",
    [0x61] = "Virgin Interactive", [0x67] = "Ocean Interactive", [0x69] = "EA (Electronic Arts)", [0x6E] = "Elite Systems", [0x6F] = "Electro Brain", [0x70] = "Infogrames",
    [0x71] = "Interplay", [0x72] = "Broderbund", [0x73] = "Sculptered Soft", [0x75] = "The Sales Curve", [0x78] = "t.hq", [0x79] = "Accolade",
    [0x7A] = "Triffix Entertainment", [0x7C] = "Microprose", [0x7F] = "Kemco", [0x80] = "Misawa Entertainment", [0x83] = "Lozc", [0x86] = "Tokuma Shoten Intermedia",
    [0x8B] = "Bullet-Proof Software", [0x8C] = "Vic Tokai", [0x8E] = "Ape", [0x8F] = "I’Max", [0x91] = "Chunsoft Co.", [0x92] = "Video System",
    [0x93] = "Tsubaraya Productions Co.", [0x95] = "Varie Corporation", [0x96] = "Yonezawa/S’Pal", [0x97] = "Kaneko", [0x99] = "Arc", [0x9A] = "Nihon Bussan",
    [0x9B] = "Tecmo", [0x9C] = "Imagineer", [0x9D] = "Banpresto", [0x9F] = "Nova", [0xA1] = "Hori Electric", [0xA2] = "Bandai",
    [0xA4] = "Konami", [0xA6] = "Kawada", [0xA7] = "Takara", [0xA9] = "Technos Japan", [0xAA] = "Broderbund", [0xAC] = "Toei Animation",
    [0xAD] = "Toho", [0xAF] = "Namco", [0xB0] = "acclaim", [0xB1] = "ASCII or Nexsoft", [0xB2] = "Bandai", [0xB4] = "Square Enix",
    [0xB6] = "HAL Laboratory", [0xB7] = "SNK", [0xB9] = "Pony Canyon", [0xBA] = "Culture Brain", [0xBB] = "Sunsoft", [0xBD] = "Sony Imagesoft",
    [0xBF] = "Sammy", [0xC0] = "Taito", [0xC2] = "Kemco", [0xC3] = "Squaresoft", [0xC4] = "Tokuma Shoten Intermedia", [0xC5] = "Data East",
    [0xC6] = "Tonkinhouse", [0xC8] = "Koei", [0xC9] = "UFL", [0xCA] = "Ultra", [0xCB] = "Vap", [0xCC] = "Use Corporation",
    [0xCD] = "Meldac", [0xCE] = ".Pony Canyon or", [0xCF] = "Angel", [0xD0] = "Taito", [0xD1] = "Sofel", [0xD2] = "Quest",
    [0xD3] = "Sigma Enterprises", [0xD4] = "ASK Kodansha Co.", [0xD6] = "Naxat Soft", [0xD7] = "Copya System", [0xD9] = "Banpresto", [0xDA] = "Tomy",
    [0xDB] = "LJN", [0xDD] = "NCS", [0xDE] = "Human", [0xDF] = "Altron", [0xE0] = "Jaleco", [0xE1] = "Towa Chiki",
    [0xE2] = "Yutaka", [0xE3] = "Varie", [0xE5] = "Epcoh", [0xE7] = "Athena", [0xE8] = "Asmik ACE Entertainment", [0xE9] = "Natsume",
    [0xEA] = "King Records", [0xEB] = "Atlus", [0xEC] = "Epic/Sony Records", [0xEE] = "IGS", [0xF0] = "A Wave", [0xF3] = "Extreme Entertainment", [0xFF] = "LJN"
};

static const char *new_licensees[] = {
    [1] = "Nintendo", [8] = "Capcom", [13] = "Electronic Arts", [18] = "Hudson Soft", [19] = "b-ai", [20] = "kss",
    [22] = "pow", [24] = "PCM Complete", [25] = "san-x", [28] = "Kemco Japan", [29] = "seta", [30] = "Viacom",
    [31] = "Nintendo", [32] = "Bandai", [33] = "Ocean/Acclaim", [34] = "Konami", [35] = "Hector", [37] = "Taito",
    [38] = "Hudson", [39] = "Banpresto", [41] = "Ubi Soft", [42] = "Atlus", [44] = "Malibu", [46] = "angel",
    [47] = "Bullet-Proof", [49] = "irem", [50] = "Absolute", [51] = "Acclaim", [52] = "Activision", [53] = "American sammy",
    [54] = "Konami", [55] = "Hi tech entertainment", [56] = "LJN", [57] = "Matchbox", [58] = "Mattel", [59] = "Milton Bradley",
    [60] = "Titus", [61] = "Virgin", [64] = "LucasArts", [67] = "Ocean", [69] = "Electronic Arts", [70] = "Infogrames",
    [71] = "Interplay", [72] = "Broderbund", [73] = "sculptured", [75] = "sci", [78] = "THQ", [79] = "Accolade",
    [80] = "misawa", [83] = "lozc", [86] = "Tokuma Shoten Intermedia", [87] = "Tsukuda Original", [91] = "Chunsoft", [92] = "Video system",
    [93] = "Ocean/Acclaim", [95] = "Varie", [96] = "Yonezawa/s’pal", [97] = "Kaneko", [99] = "Pack in soft", [0xA4] = "Konami (Yu-Gi-Oh!)"
};

static const char *get_new_licensee(gb_t *gb) {
    uint8_t code = ((gb->mmu->rom[0x0144] - 0x30) * 10) + (gb->mmu->rom[0x0145]) - 0x30;
    return new_licensees[code];
}

static const char *get_licensee(gb_t *gb) {
    uint8_t code = gb->mmu->rom[0x014B];
    return code == 0x33 ? get_new_licensee(gb) : old_licensees[code];
}

void gb_print_status(gb_t *gb) {
    gb_mmu_t *mmu = gb->mmu;
    char str_buf[30];

    const char *licensee = get_licensee(gb);
    if (licensee)
        snprintf(str_buf, 30, " by %s", licensee);

    printf("[%s] Playing %s (v%d)%s\n",
            gb->base->opts.mode == GBMULATOR_MODE_GBC ? "CGB" : "DMG",
            gb->rom_title,
            mmu->rom[0x014C],
            licensee ? str_buf : ""
    );

    if (mmu->eram_banks > 0)
        snprintf(str_buf, 17, " + %d RAM banks", mmu->eram_banks);

    printf("Cartridge using %s with %d ROM banks%s%s%s%s\n",
            mbc_names[mmu->mbc.type],
            mmu->rom_banks,
            mmu->eram_banks > 0 ? str_buf : "",
            mmu->has_battery ? " + BATTERY" : "",
            mmu->has_rtc ? " + RTC" : "",
            mmu->has_rumble ? " + RUMBLE" : ""
    );
}

uint8_t gb_link_shift_bit(gb_t *gb, uint8_t in_bit) {
    uint8_t out_bit = GET_BIT(gb->mmu->io_registers[IO_SB], 7);
    gb->mmu->io_registers[IO_SB] <<= 1;
    CHANGE_BIT(gb->mmu->io_registers[IO_SB], 0, in_bit);
    return out_bit;
}

void gb_link_data_received(gb_t *gb) {
    RESET_BIT(gb->mmu->io_registers[IO_SC], 7);
    CPU_REQUEST_INTERRUPT(gb, IRQ_SERIAL);
}

void gb_joypad_press(gb_t *gb, gbmulator_joypad_t key) {
    joypad_press(gb, key);
}

void gb_joypad_release(gb_t *gb, gbmulator_joypad_t key) {
    joypad_release(gb, key);
}

uint16_t gb_get_joypad_state(gb_t *gb) {
    gb_joypad_t *joypad = gb->joypad;

    return ((joypad->direction << 4) & 0xF0) | (joypad->action & 0x0F);
}

void gb_set_joypad_state(gb_t *gb, uint16_t state) {
    gb_joypad_t *joypad = gb->joypad;

    uint8_t direction = (state >> 4) & 0x0F;
    uint8_t action = state & 0x0F;

    // request interrupt if it is enabled and any button bit goes from released to pressed (1 -> 0)
    uint8_t direction_changed = (joypad->direction & ~direction) && !CHECK_BIT(gb->mmu->io_registers[IO_P1], 4);
    uint8_t action_changed = (joypad->action & ~action) && !CHECK_BIT(gb->mmu->io_registers[IO_P1], 5);
    if (direction_changed || action_changed)
        CPU_REQUEST_INTERRUPT(gb, IRQ_JOYPAD);

    joypad->action = action;
    joypad->direction = direction;
}

uint8_t *gb_get_save(gb_t *gb, size_t *save_length) {
    if (gb->mmu->mbc.type == MBC7) {
        *save_length = sizeof(gb->mmu->mbc.mbc7.eeprom.data);
        uint8_t *save_data = xmalloc(*save_length);
        memcpy(save_data, gb->mmu->mbc.mbc7.eeprom.data, *save_length);
        return save_data;
    }

    if (!gb->mmu->has_battery || (!gb->mmu->has_rtc && gb->mmu->eram_banks == 0)) {
        *save_length = 0;
        return NULL;
    }

    size_t rtc_len = gb->mmu->has_rtc ? 48 : 0;
    size_t eram_len = gb->mmu->eram_banks * ERAM_BANK_SIZE;
    size_t total_len = eram_len + rtc_len;
    if (save_length)
        *save_length = total_len;

    if (total_len == 0)
        return NULL;

    uint8_t *save_data = xmalloc(total_len);

    if (eram_len > 0)
        memcpy(save_data, gb->mmu->eram, eram_len);

    if (rtc_len > 0) {
        save_data[eram_len] = gb->mmu->mbc.mbc3.rtc.s;
        save_data[eram_len + 4] = gb->mmu->mbc.mbc3.rtc.m;
        save_data[eram_len + 8] = gb->mmu->mbc.mbc3.rtc.h;
        save_data[eram_len + 12] = gb->mmu->mbc.mbc3.rtc.dl;
        save_data[eram_len + 16] = gb->mmu->mbc.mbc3.rtc.dh;
        save_data[eram_len + 20] = gb->mmu->mbc.mbc3.rtc.latched_s;
        save_data[eram_len + 24] = gb->mmu->mbc.mbc3.rtc.latched_m;
        save_data[eram_len + 28] = gb->mmu->mbc.mbc3.rtc.latched_h;
        save_data[eram_len + 32] = gb->mmu->mbc.mbc3.rtc.latched_dl;
        save_data[eram_len + 36] = gb->mmu->mbc.mbc3.rtc.latched_dh;
        // fill rtc save padding bytes with 0
        for (size_t i = eram_len; i < eram_len + 40; i++)
            if (i % 4 != 0)
                save_data[i] = 0;

        time_t rtc_timestamp = time(NULL);
        for (int i = 0; i < 8; i++) // little endian serialization of rtc_timestamp
            save_data[eram_len + 40 + i] = rtc_timestamp >> (i * 8);
    }

    return save_data;
}

bool gb_load_save(gb_t *gb, uint8_t *save_data, size_t save_length) {
    if (gb->mmu->mbc.type == MBC7) {
        if (save_length != sizeof(gb->mmu->mbc.mbc7.eeprom.data))
            return 0;
        memcpy(gb->mmu->mbc.mbc7.eeprom.data, save_data, save_length);
        return 1;
    }

    // don't load save if the cartridge has no battery or there is no rtc and no eram banks
    if (!gb->mmu->has_battery || (!gb->mmu->has_rtc && gb->mmu->eram_banks == 0))
        return 0;

    size_t eram_len = gb->mmu->eram_banks * ERAM_BANK_SIZE;
    if (save_length < eram_len || save_length == 0)
        return 0;

    if (eram_len > 0)
        memcpy(gb->mmu->eram, save_data, eram_len);

    if (!gb->mmu->has_rtc)
        return 1;

    size_t rtc_len = save_length - eram_len;
    if (rtc_len != 44 && rtc_len != 48) {
        eprintf("Invalid rtc format\n");
        return 1;
    }

    // get saved rtc registers and timestamp
    uint8_t s = save_data[eram_len];
    uint8_t m = save_data[eram_len + 4];
    uint8_t h = save_data[eram_len + 8];
    uint8_t dl = save_data[eram_len + 12];
    uint8_t dh = save_data[eram_len + 16];
    gb->mmu->mbc.mbc3.rtc.latched_s = save_data[eram_len + 20];
    gb->mmu->mbc.mbc3.rtc.latched_m = save_data[eram_len + 24];
    gb->mmu->mbc.mbc3.rtc.latched_h = save_data[eram_len + 28];
    gb->mmu->mbc.mbc3.rtc.latched_dl = save_data[eram_len + 32];
    gb->mmu->mbc.mbc3.rtc.latched_dh = save_data[eram_len + 36];

    time_t rtc_timestamp = 0;
    for (int i = 0; i < (rtc_len == 48 ? 8 : 4); i++) // little endian unserialization of rtc_timestamp
        rtc_timestamp |= ((time_t) save_data[eram_len + 40 + i]) << (i * 8);

    // add elapsed time
    time_t rtc_registers_time = s + m * 60 + h * 3600 + ((dh << 8) | dl) * 86400;
    rtc_registers_time += time(NULL) - rtc_timestamp;

    // convert elapsed time back into rtc registers
    uint16_t d = rtc_registers_time / 86400;
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

    return 1;
}

gbmulator_savestate_t *gb_get_savestate(gb_t *gb, size_t *savestate_length, bool is_compressed) {
    size_t cpu_len;
    uint8_t *cpu = cpu_serialize(gb, &cpu_len);
    size_t timer_len;
    uint8_t *timer = timer_serialize(gb, &timer_len);
    size_t ppu_len;
    uint8_t *ppu = ppu_serialize(gb, &ppu_len);
    size_t mmu_len;
    uint8_t *mmu = mmu_serialize(gb, &mmu_len);

    // don't write each component length into the savestate as the only variable length is the mmu which is written
    // last and it's length can be computed using the eram_banks number and the mode (both in the header)
    size_t savestate_data_len = cpu_len + timer_len + ppu_len + mmu_len;
    gbmulator_savestate_t *savestate = xmalloc(sizeof(*savestate) + savestate_data_len);

    memcpy(savestate->identifier, SAVESTATE_STRING, sizeof(savestate->identifier));
    memcpy(savestate->rom_title, gb->rom_title, sizeof(savestate->rom_title));
    savestate->mode = gb->base->opts.mode;

    size_t offset = 0;
    memcpy(savestate->data, cpu, cpu_len);
    offset += cpu_len;
    memcpy(&savestate->data[offset], timer, timer_len);
    offset += timer_len;
    memcpy(&savestate->data[offset], ppu, ppu_len);
    offset += ppu_len;
    memcpy(&savestate->data[offset], mmu, mmu_len);

    free(cpu);
    free(timer);
    free(ppu);
    free(mmu);

    // compress savestate data if specified
    if (is_compressed) {
        uLongf dest_len = compressBound(savestate_data_len);
        uint8_t *dest = xmalloc(dest_len);
        if (compress(dest, &dest_len, savestate->data, savestate_data_len) == Z_OK) {
            savestate->is_compressed = true;
            savestate = xrealloc(savestate, dest_len + sizeof(*savestate));
            memcpy(savestate->data, dest, dest_len);
            savestate_data_len = dest_len;
        } else {
            eprintf("Couldn't compress savestate, using an uncompressed savestate instead\n");
        }
        free(dest);
    }

    *savestate_length = sizeof(*savestate) + savestate_data_len;

    return savestate;
}

bool gb_load_savestate(gb_t *gb, gbmulator_savestate_t *savestate, size_t savestate_length) {
    size_t expected_data_len = 0;
    expected_data_len += cpu_serialized_length(gb);
    expected_data_len += timer_serialized_length(gb);
    expected_data_len += ppu_serialized_length(gb);
    expected_data_len += mmu_serialized_length(gb);

    size_t savestate_data_length = savestate_length - sizeof(*savestate);
    uint8_t *savestate_data = savestate->data;

    if (savestate->is_compressed) {
        uint8_t *dest = xmalloc(expected_data_len);
        uLongf dest_len = expected_data_len;
        if (uncompress(dest, &dest_len, savestate_data, savestate_data_length) == Z_OK) {
            savestate_data_length = dest_len;
            savestate_data = dest;
        } else {
            eprintf("uncompress failure\n");
            free(dest);
            return false;
        }
    }

    if (savestate_data_length != expected_data_len) {
        eprintf("invalid savestate data length (expected: %zu; got: %zu)\n", expected_data_len, savestate_data_length);
        if (savestate->is_compressed)
            free(savestate_data);
        return false;
    }

    size_t offset = 0;
    offset += cpu_unserialize(gb, &savestate_data[offset]);
    offset += timer_unserialize(gb, &savestate_data[offset]);
    offset += ppu_unserialize(gb, &savestate_data[offset]);
    offset += mmu_unserialize(gb, &savestate_data[offset]);

    if (savestate->is_compressed)
        free(savestate_data);

    // resets apu's internal state to prevent glitchy audio if resuming from state without sound playing from state with sound playing
    apu_quit(gb);
    apu_init(gb);

    return true;
}

char *gb_get_rom_title(gb_t *gb) {
    return gb->rom_title;
}

uint8_t *gb_get_rom(gb_t *gb, size_t *rom_size) {
    if (rom_size)
        *rom_size = gb->mmu->rom_size;
    return gb->mmu->rom;
}

uint8_t gb_has_accelerometer(gb_t *gb) {
    return gb->mmu->mbc.type == MBC7;
}

uint8_t gb_has_camera(gb_t *gb) {
    return gb->mmu->mbc.type == CAMERA;
}

void gb_set_palette(gb_t *gb, gb_color_palette_t palette) {
    gb->dmg_palette = palette;
}
