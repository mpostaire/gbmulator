#include <stdio.h>
#include <stdlib.h>

#include "gba_priv.h"

static const char *makers[] = {
    [0x01] = "Nintendo"
};

void gba_step(gba_t *gba) {
    gba_cpu_step(gba);
    gba_ppu_step(gba);
    gba_tmr_step(gba);
    gba_dma_step(gba);
}

gba_t *gba_init(const uint8_t *rom, size_t rom_size, gbmulator_options_t *opts) {
    gba_t *gba = xcalloc(1, sizeof(*gba));

    if (!gba_bus_init(gba, rom, rom_size)) {
        free(gba);
        return NULL;
    }

    gba_cpu_init(gba);
    gba_ppu_init(gba);
    gba_tmr_init(gba);
    gba_dma_init(gba);

    if (opts) {
        gba->on_new_frame = opts->on_new_frame;
        gba->on_new_sample = opts->on_new_sample;
        gba->on_accelerometer_request = opts->on_accelerometer_request;
        gba->on_camera_capture_image = opts->on_camera_capture_image;
    }

    return gba;
}

void gba_quit(gba_t *gba) {
    gba_cpu_quit(gba);
    gba_bus_quit(gba);
    gba_ppu_quit(gba);
    gba_tmr_quit(gba);
    gba_dma_quit(gba);
    free(gba);
}

void gba_print_status(gba_t *gba) {
    uint8_t language = gba->bus->game_rom[0xAF];
    uint16_t maker_code = ((gba->bus->game_rom[0xB0] - 0x30) * 10) + (gba->bus->game_rom[0xB1] - 0x30); // maker_code is 2 chars
    uint8_t version = gba->bus->game_rom[0xBC];

    char maker_buf[32];
    if (maker_code < sizeof(makers))
        snprintf(maker_buf, sizeof(maker_buf), "%s", makers[maker_code]);
    else
        snprintf(maker_buf, sizeof(maker_buf), "%c%c", gba->bus->game_rom[0xB0], gba->bus->game_rom[0xB1]);

    char *language_str;
    switch (language) {
    case 'J':
        language_str = "Japan";
        break;
    case 'P':
        language_str = "Europe/Elsewhere";
        break;
    case 'F':
        language_str = "French";
        break;
    case 'S':
        language_str = "Spanish";
        break;
    case 'E':
        language_str = "USA/English";
        break;
    case 'D':
        language_str = "German";
        break;
    case 'I':
        language_str = "Italian";
        break;
    default:
        language_str = "Unknown";
    }

    printf("Playing %s (v%d) by %s (language: %s)\n", *gba->rom_title ? gba->rom_title : "Unknown", version, maker_buf, language_str);
}

char *gba_get_rom_title(gba_t *gba) {
    return gba->rom_title;
}

void gba_set_joypad_state(gba_t *gba, uint16_t state) {
    gba->bus->io_regs[IO_KEYINPUT] = state & 0x03FF;
    // TODO interrupts
}

uint8_t *gba_get_save(gba_t *gba, size_t *save_length) {
    // TODO
    return NULL;
}

bool gba_load_save(gba_t *gba, uint8_t *save_data, size_t save_length) {
    // TODO
    return false;
}

uint8_t *gba_get_savestate(gba_t *gba, size_t *length, bool is_compressed) {
    // TODO
    return NULL;
}

bool gba_load_savestate(gba_t *gba, uint8_t *data, size_t length) {
    // TODO
    return false;
}

uint8_t *gba_get_rom(gba_t *gba, size_t *rom_size) {
    *rom_size = gba->bus->rom_size;
    return gba->bus->game_rom;
}
