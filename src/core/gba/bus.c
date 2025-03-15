#include <stdlib.h>

#include "gba_priv.h"

#define ALIGN_ADDRESS(address, n) ((address) & (~((n) - 1))) // TODO maybe wrong


const char *makers[256] = {
    [0x01] = "Nintendo"
};

static bool gba_is_rom_valid(const uint8_t *rom, size_t rom_size) {
    if (rom_size < 0xBF)
        return false;

    // uint8_t entrypoint = rom[0x00];

    char title[13]; // title is max 12 chars
    memcpy(title, &rom[0xA0], sizeof(title));
    title[12] = '\0';

    // uint8_t game_type = rom[0xAC];
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

void gba_bus_select(gba_t *gba, uint32_t address) {
    printf("bus addr select: 0x%08X\n", address);

    gba_bus_t *bus = gba->bus;

    bus->address = address;

    switch (address) {
    case BUS_BIOS_ROM ... BUS_BIOS_ROM_UNUSED - 1:
        bus->selected_mem_ptr = (uint8_t *) &bus->bios_rom;
        bus->selected_mem_offset = BUS_BIOS_ROM;
        bus->is_writeable = false;
        break;
    case BUS_WRAM_BOARD ... BUS_WRAM_BOARD_UNUSED - 1:
        bus->selected_mem_ptr = (uint8_t *) &bus->wram_board;
        bus->selected_mem_offset = BUS_WRAM_BOARD;
        bus->is_writeable = true;
        break;
    case BUS_WRAM_CHIP ... BUS_WRAM_CHIP_UNUSED - 1:
        bus->selected_mem_ptr = (uint8_t *) &bus->wram_chip;
        bus->selected_mem_offset = BUS_WRAM_CHIP;
        bus->is_writeable = true;
        break;
    case BUS_IO_REGS ... BUS_IO_REGS_UNUSED - 1:
        bus->selected_mem_ptr = (uint8_t *) &bus->io_regs;
        bus->selected_mem_offset = BUS_IO_REGS;
        bus->is_writeable = true;
        break;
    case BUS_PALETTE_RAM ... BUS_PALETTE_RAM_UNUSED - 1:
        bus->selected_mem_ptr = (uint8_t *) &bus->palette_ram;
        bus->selected_mem_offset = BUS_PALETTE_RAM;
        bus->is_writeable = true;
        break;
    case BUS_VRAM ... BUS_VRAM_UNUSED - 1:
        bus->selected_mem_ptr = (uint8_t *) &bus->vram;
        bus->selected_mem_offset = BUS_VRAM;
        bus->is_writeable = true;
        break;
    case BUS_OAM ... BUS_OAM_UNUSED - 1:
        bus->selected_mem_ptr = (uint8_t *) &bus->oam;
        bus->selected_mem_offset = BUS_OAM;
        bus->is_writeable = true;
        break;
    case BUS_GAME_ROM0 ... BUS_GAME_ROM1 - 1:
        bus->selected_mem_ptr = (uint8_t *) &bus->game_rom;
        bus->selected_mem_offset = BUS_GAME_ROM0;
        bus->is_writeable = false;
        break;
    case BUS_GAME_ROM1 ... BUS_GAME_ROM2 - 1:
        bus->selected_mem_ptr = (uint8_t *) &bus->game_rom;
        bus->selected_mem_offset = BUS_GAME_ROM1;
        bus->is_writeable = false;
        break;
    case BUS_GAME_ROM2 ... BUS_GAME_SRAM - 1:
        bus->selected_mem_ptr = (uint8_t *) &bus->game_rom;
        bus->selected_mem_offset = BUS_GAME_ROM2;
        bus->is_writeable = false;
        break;
    case BUS_GAME_SRAM ... BUS_GAME_UNUSED - 1:
        bus->selected_mem_ptr = (uint8_t *) &bus->game_sram;
        bus->selected_mem_offset = BUS_GAME_SRAM;
        bus->is_writeable = true;
        break;
    default:
        bus->selected_mem_ptr = NULL;
        bus->selected_mem_offset = 0;
        bus->is_writeable = false;
        break;
    }
}

uint8_t gba_bus_read_byte(gba_t *gba) {
    gba_bus_t *bus = gba->bus;

    uint8_t ret = 0xFF;
    if (!bus->selected_mem_ptr)
        return ret;

    uint32_t address = ALIGN_ADDRESS(bus->address - bus->selected_mem_offset, sizeof(ret));
    memcpy(&ret, bus->selected_mem_ptr + address, sizeof(ret));
    return ret;
}
uint16_t gba_bus_read_half(gba_t *gba) {
    gba_bus_t *bus = gba->bus;

    uint16_t ret = 0xFFFF;
    if (!bus->selected_mem_ptr)
        return ret;

    uint32_t address = ALIGN_ADDRESS(bus->address - bus->selected_mem_offset, sizeof(ret));
    memcpy(&ret, bus->selected_mem_ptr + address, sizeof(ret));
    return ret;
}
uint32_t gba_bus_read_word(gba_t *gba) {
    gba_bus_t *bus = gba->bus;

    uint32_t ret = 0xFFFFFFFF;
    if (!bus->selected_mem_ptr)
        return ret;

    uint32_t address = ALIGN_ADDRESS(bus->address - bus->selected_mem_offset, sizeof(ret));
    memcpy(&ret, bus->selected_mem_ptr + address, sizeof(ret));
    return ret;
}

// TODO data type may be wrong
void gba_bus_write(gba_t *gba, uint32_t data) {
    if (gba->bus->is_writeable) {
        eprintf("bus address 0x%08X is read only", gba->bus->address);
        return;
    } else if (!gba->bus->selected_mem_ptr) {
        eprintf("bus address 0x%08X is unmapped memory", gba->bus->address);
        return;
    }

    eprintf("bus write of 0x%08X at 0x%08X", data, gba->bus->address);

    // TODO special cases like io registers
    gba->bus->selected_mem_ptr[gba->bus->selected_mem_offset] = data;
}

gba_bus_t *gba_bus_init(const uint8_t *rom, size_t rom_size) {
    if (!gba_is_rom_valid(rom, rom_size))
        return NULL;

    // TODO do not load bios from hardcoded file path
    FILE *f = fopen("src/bootroms/gba/gba_bios.bin", "r");
    if (!f) {
        return NULL;
    }

    gba_bus_t *bus = xcalloc(1, sizeof(*bus));
    memcpy(bus->game_rom, rom, rom_size);

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    fread(bus->bios_rom, sz, 1, f);
    fclose(f);

    return bus;
}

void gba_bus_quit(gba_bus_t *bus) {
    free(bus);
}
