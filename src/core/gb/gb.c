#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <zlib.h>

#include "gb_priv.h"

#define SAVESTATE_STRING EMULATOR_NAME"-sav"

typedef struct __attribute__((packed)) {
    char identifier[sizeof(SAVESTATE_STRING)];
    char rom_title[16];
    uint8_t is_cgb; // bit 7 set --> savestate data is compressed
} savestate_header_t;

gbmulator_options_t defaults_opts = {
    .mode = GBMULATOR_MODE_GBC,
    .disable_color_correction = 0,
    .palette = PPU_COLOR_PALETTE_ORIG,
    .apu_speed = 1.0f,
    .apu_sampling_rate = 44100,
    .on_new_sample = NULL,
    .on_new_frame = NULL,
    .on_accelerometer_request = NULL,
    .on_camera_capture_image = NULL
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

#define N_REWIND_STATES (8 * GB_FRAMES_PER_SECOND)
static size_t get_savestate_expected_len(gb_t *gb, size_t *cpu_len, size_t *timer_len, size_t *ppu_len, size_t *mmu_len);

void rewind_push(gb_t *gb) {
    // eprintf("rewind_push\n");
    gb->rewind_stack.head = (gb->rewind_stack.head + 1) % N_REWIND_STATES;
    if (gb->rewind_stack.len < N_REWIND_STATES)
        gb->rewind_stack.len++;

    uint8_t *savestate_data = gb_get_savestate(gb, &gb->rewind_stack.state_size, 0); // TODO specify the dest buffer to avoid redundant memcpy
    // eprintf("gb_get_savestate push head=%ld sz=%ld expected_sz=%ld\n", gb->rewind_stack.head, gb->rewind_stack.state_size, get_savestate_expected_len(gb, NULL, NULL, NULL, NULL));
    memcpy(&gb->rewind_stack.states[gb->rewind_stack.head * gb->rewind_stack.state_size], savestate_data, gb->rewind_stack.state_size);
    free(savestate_data);
    // eprintf("ok push %ld\n", gb->rewind_stack.head);
}

void rewind_pop(gb_t *gb) {
    eprintf("rewind pop\n");
    if (gb->rewind_stack.len == 0)
        return;

    eprintf("gb_load_savestate pop %ld\n", gb->rewind_stack.head);
    gb_load_savestate(gb, &gb->rewind_stack.states[gb->rewind_stack.head * gb->rewind_stack.state_size], gb->rewind_stack.state_size);

    gb->rewind_stack.head = gb->rewind_stack.head == 0 ? (N_REWIND_STATES - 1) : gb->rewind_stack.head - 1;
    gb->rewind_stack.len--;
    // if len == 0, set head back to -1??
}

void gb_rewind(gb_t *gb) {
    // TODO while rewind button is pressed, pause the emulation (do not step)
    rewind_pop(gb);
}

static inline void gb_step_linked(gb_t *gb, uint8_t step_linked_device) {
    if (gb->rewind_stack.states) {
        static size_t step_counter = 0;
        step_counter++;
        if (step_counter == GB_CPU_STEPS_PER_FRAME) {
            step_counter = 0;
            rewind_push(gb);
        }
    }

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

    if (step_linked_device) {
        switch (gb->link->linked_device.type) {
        case LINK_TYPE_NONE: break;
        case LINK_TYPE_GB: gb_step_linked(gb->link->linked_device.device, 0); break;
        case LINK_TYPE_PRINTER: gb_printer_step(gb->link->linked_device.device); break;
        }
    }
}

void gb_step(gb_t *gb) {
    gb_step_linked(gb, 1);
}

int gb_is_rom_valid(const uint8_t *rom) {
    return parse_header_mbc_byte(rom[0x0147], NULL, NULL, NULL, NULL, NULL) && validate_header_checksum(rom);
}

static size_t get_savestate_expected_len(gb_t *gb, size_t *cpu_len, size_t *timer_len, size_t *ppu_len, size_t *mmu_len) {
    size_t cpu, timer, ppu, mmu;
    cpu = cpu_serialized_length(gb);
    timer = timer_serialized_length(gb);
    ppu = ppu_serialized_length(gb);
    mmu = mmu_serialized_length(gb);

    if (cpu_len)
        *cpu_len = cpu;
    if (timer_len)
        *timer_len = timer;
    if (ppu_len)
        *ppu_len = ppu;
    if (mmu_len)
        *mmu_len = mmu;

    return cpu + timer + ppu + mmu + sizeof(savestate_header_t);
}

gb_t *gb_init(const uint8_t *rom, size_t rom_size, gbmulator_options_t *opts) {
    gb_t *gb = xcalloc(1, sizeof(*gb));
    gb_set_options(gb, opts);

    if (!mmu_init(gb, rom, rom_size)) {
        free(gb);
        return NULL;
    }
    cpu_init(gb);
    apu_init(gb);
    ppu_init(gb);
    timer_init(gb);
    link_init(gb);
    joypad_init(gb);

    gb->rewind_stack.states = xmalloc(N_REWIND_STATES * get_savestate_expected_len(gb, NULL, NULL, NULL, NULL));
    gb->rewind_stack.head = -1;

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

    if (gb->rewind_stack.states)
        free(gb->rewind_stack.states);

    free(gb);
}

void gb_reset(gb_t *gb, bool is_cgb) {
    size_t rom_size = gb->mmu->rom_size;
    uint8_t *rom = xmalloc(rom_size);
    memcpy(rom, gb->mmu->rom, rom_size);

    gbmulator_options_t opts;
    gb_get_options(gb, &opts);
    opts.mode = is_cgb ? GBMULATOR_MODE_GBC : GBMULATOR_MODE_GB;
    gb_set_options(gb, &opts);

    size_t save_len;
    uint8_t *save_data = gb_get_save(gb, &save_len);

    cpu_quit(gb);
    apu_quit(gb);
    mmu_quit(gb);
    ppu_quit(gb);
    timer_quit(gb);
    link_quit(gb);
    joypad_quit(gb);

    mmu_init(gb, rom, rom_size);
    cpu_init(gb);
    apu_init(gb);
    ppu_init(gb);
    timer_init(gb);
    link_init(gb);
    joypad_init(gb);

    if (save_data)
        gb_load_save(gb, save_data, save_len);

    free(rom);
}

static const char *old_licensees[] = {
    [0x01] = "Nintendo",[0x08] = "Capcom",[0x09] = "Hot-B",[0x0A] = "Jaleco",[0x0B] = "Coconuts Japan",[0x0C] = "Elite Systems",
    [0x13] = "EA (Electronic Arts)",[0x18] = "Hudsonsoft",[0x19] = "ITC Entertainment",[0x1A] = "Yanoman",[0x1D] = "Japan Clary",[0x1F] = "Virgin Interactive",
    [0x24] = "PCM Complete",[0x25] = "San-X",[0x28] = "Kotobuki Systems",[0x29] = "Seta",[0x30] = "Infogrames",[0x31] = "Nintendo",
    [0x32] = "Bandai",[0x34] = "Konami",[0x35] = "HectorSoft",[0x38] = "Capcom",[0x39] = "Banpresto",[0x3C] = ".Entertainment i",
    [0x3E] = "Gremlin",[0x41] = "Ubisoft",[0x42] = "Atlus",[0x44] = "Malibu",[0x46] = "Angel",[0x47] = "Spectrum Holoby",
    [0x49] = "Irem",[0x4A] = "Virgin Interactive",[0x4D] = "Malibu",[0x4F] = "U.S. Gold",[0x50] = "Absolute",[0x51] = "Acclaim",
    [0x52] = "Activision",[0x53] = "American Sammy",[0x54] = "GameTek",[0x55] = "Park Place",[0x56] = "LJN",[0x57] = "Matchbox",
    [0x59] = "Milton Bradley",[0x5A] = "Mindscape",[0x5B] = "Romstar",[0x5C] = "Naxat Soft",[0x5D] = "Tradewest",[0x60] = "Titus",
    [0x61] = "Virgin Interactive",[0x67] = "Ocean Interactive",[0x69] = "EA (Electronic Arts)",[0x6E] = "Elite Systems",[0x6F] = "Electro Brain",[0x70] = "Infogrames",
    [0x71] = "Interplay",[0x72] = "Broderbund",[0x73] = "Sculptered Soft",[0x75] = "The Sales Curve",[0x78] = "t.hq",[0x79] = "Accolade",
    [0x7A] = "Triffix Entertainment",[0x7C] = "Microprose",[0x7F] = "Kemco",[0x80] = "Misawa Entertainment",[0x83] = "Lozc",[0x86] = "Tokuma Shoten Intermedia",
    [0x8B] = "Bullet-Proof Software",[0x8C] = "Vic Tokai",[0x8E] = "Ape",[0x8F] = "I’Max",[0x91] = "Chunsoft Co.",[0x92] = "Video System",
    [0x93] = "Tsubaraya Productions Co.",[0x95] = "Varie Corporation",[0x96] = "Yonezawa/S’Pal",[0x97] = "Kaneko",[0x99] = "Arc",[0x9A] = "Nihon Bussan",
    [0x9B] = "Tecmo",[0x9C] = "Imagineer",[0x9D] = "Banpresto",[0x9F] = "Nova",[0xA1] = "Hori Electric",[0xA2] = "Bandai",
    [0xA4] = "Konami",[0xA6] = "Kawada",[0xA7] = "Takara",[0xA9] = "Technos Japan",[0xAA] = "Broderbund",[0xAC] = "Toei Animation",
    [0xAD] = "Toho",[0xAF] = "Namco",[0xB0] = "acclaim",[0xB1] = "ASCII or Nexsoft",[0xB2] = "Bandai",[0xB4] = "Square Enix",
    [0xB6] = "HAL Laboratory",[0xB7] = "SNK",[0xB9] = "Pony Canyon",[0xBA] = "Culture Brain",[0xBB] = "Sunsoft",[0xBD] = "Sony Imagesoft",
    [0xBF] = "Sammy",[0xC0] = "Taito",[0xC2] = "Kemco",[0xC3] = "Squaresoft",[0xC4] = "Tokuma Shoten Intermedia",[0xC5] = "Data East",
    [0xC6] = "Tonkinhouse",[0xC8] = "Koei",[0xC9] = "UFL",[0xCA] = "Ultra",[0xCB] = "Vap",[0xCC] = "Use Corporation",
    [0xCD] = "Meldac",[0xCE] = ".Pony Canyon or",[0xCF] = "Angel",[0xD0] = "Taito",[0xD1] = "Sofel",[0xD2] = "Quest",
    [0xD3] = "Sigma Enterprises",[0xD4] = "ASK Kodansha Co.",[0xD6] = "Naxat Soft",[0xD7] = "Copya System",[0xD9] = "Banpresto",[0xDA] = "Tomy",
    [0xDB] = "LJN",[0xDD] = "NCS",[0xDE] = "Human",[0xDF] = "Altron",[0xE0] = "Jaleco",[0xE1] = "Towa Chiki",
    [0xE2] = "Yutaka",[0xE3] = "Varie",[0xE5] = "Epcoh",[0xE7] = "Athena",[0xE8] = "Asmik ACE Entertainment",[0xE9] = "Natsume",
    [0xEA] = "King Records",[0xEB] = "Atlus",[0xEC] = "Epic/Sony Records",[0xEE] = "IGS",[0xF0] = "A Wave",[0xF3] = "Extreme Entertainment", [0xFF] = "LJN"
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
            gb->is_cgb ? "CGB" : "DMG",
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

void gb_link_connect_gb(gb_t *gb, gb_t *other_gb) {
    gb_link_disconnect(gb);

    gb->link->linked_device.device = other_gb;
    gb->link->linked_device.type = LINK_TYPE_GB;
    gb->link->linked_device.shift_bit = gb_linked_shift_bit;
    gb->link->linked_device.data_received = gb_linked_data_received;

    other_gb->link->linked_device.device = gb;
    other_gb->link->linked_device.type = LINK_TYPE_GB;
    other_gb->link->linked_device.shift_bit = gb_linked_shift_bit;
    other_gb->link->linked_device.data_received = gb_linked_data_received;
}

void gb_link_connect_printer(gb_t *gb, gb_printer_t *printer) {
    gb_link_disconnect(gb);

    gb->link->linked_device.device = printer;
    gb->link->linked_device.type = LINK_TYPE_PRINTER;
    gb->link->linked_device.shift_bit = gb_printer_linked_shift_bit;
    gb->link->linked_device.data_received = gb_printer_linked_data_received;
}

void gb_link_disconnect(gb_t *gb) {
    if (gb->link->linked_device.type == LINK_TYPE_GB) {
        gb_t *linked_gb = gb->link->linked_device.device;
        linked_gb->link->linked_device.device = NULL;
        linked_gb->link->linked_device.type = LINK_TYPE_NONE;
        linked_gb->link->linked_device.shift_bit = NULL;
        linked_gb->link->linked_device.data_received = NULL;
    }

    gb->link->linked_device.device = NULL;
    gb->link->linked_device.type = LINK_TYPE_NONE;
    gb->link->linked_device.shift_bit = NULL;
    gb->link->linked_device.data_received = NULL;
}

uint8_t gb_ir_connect(gb_t *gb, gb_t *other_gb) {
    if (!gb->is_cgb && gb->mmu->mbc.type != HuC1 && gb->mmu->mbc.type != HuC3)
        return 0;
    if (!other_gb->is_cgb && other_gb->mmu->mbc.type != HuC1 && other_gb->mmu->mbc.type != HuC3)
        return 0;

    gb_ir_disconnect(gb);

    gb->ir_gb = other_gb;
    other_gb->ir_gb = gb;

    return 1;
}

void gb_ir_disconnect(gb_t *gb) {
    if (!gb->is_cgb)
        return;

    if (gb->ir_gb && gb->ir_gb->is_cgb)
        gb->ir_gb->ir_gb = NULL;

    gb->ir_gb = NULL;
}

void gb_joypad_press(gb_t *gb, joypad_button_t key) {
    joypad_press(gb, key);
}

void gb_joypad_release(gb_t *gb, joypad_button_t key) {
    joypad_release(gb, key);
}

void gb_set_joypad_state(gb_t *gb, uint16_t state) {
    gb_joypad_t *joypad = gb->joypad;

    uint8_t direction = state & 0x0F;
    uint8_t action = (state >> 4) & 0x0F;

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

uint8_t *gb_get_savestate(gb_t *gb, size_t *length, bool is_compressed) {
    // make savestate header
    savestate_header_t *savestate_header = xmalloc(sizeof(*savestate_header));
    savestate_header->is_cgb = gb->is_cgb;
    memcpy(savestate_header->identifier, SAVESTATE_STRING, sizeof(savestate_header->identifier));
    memcpy(savestate_header->rom_title, gb->rom_title, sizeof(savestate_header->rom_title));

    // make savestate data
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
    uint8_t *savestate_data = xmalloc(savestate_data_len);
    size_t offset = 0;
    memcpy(savestate_data, cpu, cpu_len);
    offset += cpu_len;
    memcpy(&savestate_data[offset], timer, timer_len);
    offset += timer_len;
    memcpy(&savestate_data[offset], ppu, ppu_len);
    offset += ppu_len;
    memcpy(&savestate_data[offset], mmu, mmu_len);

    free(cpu);
    free(timer);
    free(ppu);
    free(mmu);

    // compress savestate data if specified
    if (is_compressed) {
        uLongf dest_len = compressBound(savestate_data_len);
        uint8_t *dest = xmalloc(dest_len);
        if (compress(dest, &dest_len, savestate_data, savestate_data_len) == Z_OK) {
            SET_BIT(savestate_header->is_cgb, 7);
            free(savestate_data);
            savestate_data_len = dest_len;
            savestate_data = dest;
        } else {
            eprintf("Couldn't compress savestate, using an uncompressed savestate instead\n");
            free(dest);
        }
    }

    // assemble savestate header and savestate data
    *length = sizeof(savestate_header_t) + savestate_data_len;
    uint8_t *savestate = xmalloc(*length);
    memcpy(savestate, savestate_header, sizeof(savestate_header_t));
    memcpy(savestate + sizeof(savestate_header_t), savestate_data, savestate_data_len);

    free(savestate_header);
    free(savestate_data);

    return savestate;
}

bool gb_load_savestate(gb_t *gb, const uint8_t *data, size_t length) {
    if (length <= sizeof(savestate_header_t)) {
        eprintf("invalid savestate length (%zu)\n", length);
        return 0;
    }

    savestate_header_t *savestate_header = xmalloc(sizeof(*savestate_header));
    memcpy(savestate_header, data, sizeof(savestate_header_t));

    if (strncmp(savestate_header->identifier, SAVESTATE_STRING, sizeof(SAVESTATE_STRING))) {
        eprintf("invalid format %s\n", savestate_header->identifier);
        free(savestate_header);
        return 0;
    }
    if (strncmp(savestate_header->rom_title, gb->rom_title, sizeof(savestate_header->rom_title))) {
        eprintf("rom title mismatch (expected: '%.16s'; got: '%.16s')\n", gb->rom_title, savestate_header->rom_title);
        free(savestate_header);
        return 0;
    }

    if ((savestate_header->is_cgb & 0x03) != gb->is_cgb)
        gb_reset(gb, savestate_header->is_cgb & 0x03);

    size_t savestate_data_len = length - sizeof(savestate_header_t);
    uint8_t *savestate_data = xmalloc(savestate_data_len);
    memcpy(savestate_data, data + sizeof(savestate_header_t), savestate_data_len);

    size_t cpu_len, timer_len, ppu_len, mmu_len;
    size_t expected_data_len = get_savestate_expected_len(gb, &cpu_len, &timer_len, &ppu_len, &mmu_len) - sizeof(savestate_header_t);

    if (CHECK_BIT(savestate_header->is_cgb, 7)) {
        uint8_t *dest = xmalloc(expected_data_len);
        uLongf dest_len = expected_data_len;
        if (uncompress(dest, &dest_len, savestate_data, savestate_data_len) == Z_OK) {
            free(savestate_data);
            savestate_data_len = dest_len;
            savestate_data = dest;
        } else {
            eprintf("uncompress failure\n");
            free(dest);
            free(savestate_header);
            free(savestate_data);
            return 0;
        }
    }

    if (savestate_data_len != expected_data_len) {
        eprintf("invalid savestate data length (expected: %zu; got: %zu)\n", expected_data_len, savestate_data_len);
        free(savestate_header);
        free(savestate_data);
        return 0;
    }

    size_t offset = 0;
    cpu_unserialize(gb, savestate_data);
    offset += cpu_len;
    timer_unserialize(gb, &savestate_data[offset]);
    offset += timer_len;
    ppu_unserialize(gb, &savestate_data[offset]);
    offset += ppu_len;
    mmu_unserialize(gb, &savestate_data[offset]);

    free(savestate_header);
    free(savestate_data);

    // resets apu's internal state to prevent glitchy audio if resuming from state without sound playing from state with sound playing
    apu_quit(gb);
    apu_init(gb);

    return gb->is_cgb;
}

char *gb_get_rom_title(gb_t *gb) {
    return gb->rom_title;
}

uint8_t *gb_get_rom(gb_t *gb, size_t *rom_size) {
    if (rom_size)
        *rom_size = gb->mmu->rom_size;
    return gb->mmu->rom;
}

void gb_get_options(gb_t *gb, gbmulator_options_t *opts) {
    opts->mode = gb->is_cgb ? GBMULATOR_MODE_GBC : GBMULATOR_MODE_GB;
    opts->disable_color_correction = gb->disable_cgb_color_correction;
    opts->apu_speed = gb->apu_speed;
    opts->apu_sampling_rate = gb->apu_sampling_rate;
    opts->palette = gb->dmg_palette;
    opts->on_new_sample = gb->on_new_sample;
    opts->on_new_frame = gb->on_new_frame;
    opts->on_accelerometer_request = gb->on_accelerometer_request;
    opts->on_camera_capture_image = gb->on_camera_capture_image;
}

void gb_set_options(gb_t *gb, gbmulator_options_t *opts) {
    if (!opts)
        opts = &defaults_opts;

    // allow changes of mode and apu_sampling_rate only once (inside gb_init())
    if (!gb->cpu) {
        gb->is_cgb = opts->mode == GBMULATOR_MODE_GBC;
        gb->cgb_mode_enabled = gb->is_cgb;
        gb->apu_sampling_rate = opts->apu_sampling_rate == 0 ? defaults_opts.apu_sampling_rate : opts->apu_sampling_rate;
    }

    gb->disable_cgb_color_correction = opts->disable_color_correction;
    gb->apu_speed = opts->apu_speed < 1.0f ? defaults_opts.apu_speed : opts->apu_speed;
    gb->dmg_palette = opts->palette >= 0 && opts->palette < PPU_COLOR_PALETTE_MAX ? opts->palette : defaults_opts.palette;
    gb->on_new_frame = opts->on_new_frame;
    gb->on_new_sample = opts->on_new_sample;
    gb->on_accelerometer_request = opts->on_accelerometer_request;
    gb->on_camera_capture_image = opts->on_camera_capture_image;
}

bool gb_is_cgb(gb_t *gb) {
    return gb->is_cgb;
}

uint16_t gb_get_cartridge_checksum(gb_t *gb) {
    uint16_t checksum = 0;
    for (unsigned int i = 0; i < gb->mmu->rom_size; i += 2)
        checksum = checksum - (gb->mmu->rom[i] + gb->mmu->rom[i + 1]) - 1;
    return checksum;
}

void gb_set_apu_speed(gb_t *gb, float speed) {
    gbmulator_options_t opts;
    gb_get_options(gb, &opts);
    opts.apu_speed = speed;
    gb_set_options(gb, &opts);
}

void gb_set_palette(gb_t *gb, gb_color_palette_t palette) {
    gbmulator_options_t opts;
    gb_get_options(gb, &opts);
    opts.palette = palette;
    gb_set_options(gb, &opts);
}

uint8_t gb_has_accelerometer(gb_t *gb) {
    return gb->mmu->mbc.type == MBC7;
}

uint8_t gb_has_camera(gb_t *gb) {
    return gb->mmu->mbc.type == CAMERA;
}
