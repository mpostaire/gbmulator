#include <stdio.h>
#include <stdlib.h>

#include "gba_priv.h"

const char *makers[256] = {
    [0x01] = "Nintendo"
};

bool gba_is_rom_valid(uint8_t *rom, size_t rom_size) {
    if (rom_size < 0xBF)
        return false;

    // uint8_t entrypoint = rom[0x00];

    char title[13]; // title is max 12 chars
    memcpy(title, &rom[0xA0], sizeof(title));
    title[12] = '\0';

    uint8_t game_type = rom[0xAC];
    // switch (game_type) {
    //     case 'A':
    //     case 'B':
    //     case 'C':
    //         break;
    //     case 'F':
    //     case 'K':
    //     case 'P':
    //     case 'R':
    //     case 'U':
    //     case 'V':
    //         eprintf("game type '%c' is not implemented yet", game_type);
    //         return false;
    //     default:
    //         eprintf("invalid game type: %c", game_type);
    //         return false;
    // }

    char short_title[3]; // short_title is 2 chars
    memcpy(short_title, &rom[0xAD], sizeof(short_title));
    short_title[2] = '\0';

    uint8_t language = rom[0xAF];

    uint16_t maker_code = ((rom[0xB0] - 0x30) * 10) + (rom[0xB1] - 0x30); // maker_code is 2 chars

    if (rom[0xB2] != 0x96 || rom[0xB3] != 0x00)
        return false;

    for (int i = 0xB5; i < 0xBC; i++)
        if (rom[i] != 0x00)
            return false;

    uint8_t version = rom[0xBC];

    uint8_t checksum = 0;
    for (int i = 0xA0; i < 0xBC; i++)
        checksum -= rom[i];
    checksum -= 0x19;

    if (rom[0xBD] != checksum) {
        eprintf("Invalid cartridge header checksum");
        return false;
    }

    if (rom[0xBE] != 0x00 && rom[0xBF] != 0x00)
        return false;

    // TODO multiboot entries

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

    // TODO this should not be in this function
    printf("Playing %s (%s v%d) by %s (language: %s)\n", title, short_title, version, maker_name, language_str);

    return true;
}

void gba_step(gba_t *gba) {
    gba_cpu_step(gba);
}

gba_t *gba_init(uint8_t *rom, size_t rom_size, uint8_t *bios, size_t bios_size) {
    gba_t *gba = xcalloc(1, sizeof(*gba));

    gba->bus = gba_bus_init(rom, rom_size, bios, bios_size);
    if (!gba->bus) {
        free(gba);
        return NULL;
    }

    gba->cpu = gba_cpu_init();

    return gba;
}

void gba_quit(gba_t *gba) {
    gba_cpu_quit(gba->cpu);
    gba_bus_quit(gba->bus);
    free(gba);
}
