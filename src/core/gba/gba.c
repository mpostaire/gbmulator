#include <stdio.h>
#include <stdlib.h>

#include "gba_priv.h"

static const char *makers[256] = {
    [0x01] = "Nintendo"
};

void gba_step(gba_t *gba) {
    gba_cpu_step(gba);
    gba_ppu_step(gba);
}

gba_t *gba_init(const uint8_t *rom, size_t rom_size, gbmulator_options_t *opts) {
    gba_t *gba = xcalloc(1, sizeof(*gba));

    if (!gba_bus_init(gba, rom, rom_size)) {
        free(gba);
        return NULL;
    }

    gba_cpu_init(gba);
    gba_ppu_init(gba);

    gba->on_new_frame = opts->on_new_frame;
    gba->on_new_sample = opts->on_new_sample;
    gba->on_accelerometer_request = opts->on_accelerometer_request;
    gba->on_camera_capture_image = opts->on_camera_capture_image;

    return gba;
}

void gba_quit(gba_t *gba) {
    gba_cpu_quit(gba->cpu);
    gba_bus_quit(gba->bus);
    gba_ppu_quit(gba->ppu);
    free(gba);
}

void gba_print_status(gba_t *gba) {
    uint8_t language = gba->bus->game_rom[0xAF];
    uint16_t maker_code = ((gba->bus->game_rom[0xB0] - 0x30) * 10) + (gba->bus->game_rom[0xB1] - 0x30); // maker_code is 2 chars
    uint8_t version = gba->bus->game_rom[0xBC];

    const char *maker_name = makers[maker_code];
    if (!maker_name)
        maker_name = "Unknown";

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

    printf("Playing %s (v%d) by %s (language: %s)\n", *gba->rom_title ? gba->rom_title : "Unknown", version, maker_name, language_str);
}

char *gba_get_rom_title(gba_t *gba) {
    return gba->rom_title;
}

void gba_set_joypad_state(gba_t *gba, uint16_t state) {
    // TODO
}
