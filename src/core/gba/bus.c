#include <stdlib.h>

#include "gba_priv.h"

#define ALIGN_ADDRESS(address, n) ((address) & (~((n) - 1))) // TODO maybe wrong

static bool gba_parse_cartridge(gba_t *gba) {
    // uint8_t entrypoint = gba->bus->game_rom[0x00];

    memcpy(gba->rom_title, &gba->bus->game_rom[0xA0], sizeof(gba->rom_title));
    gba->rom_title[12] = '\0';

    // uint8_t game_type = gba->bus->game_rom[0xAC];
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

    // char short_title[3]; // short_title is 2 chars
    // memcpy(short_title, &gba->bus->game_rom[0xAD], sizeof(short_title));
    // short_title[2] = '\0';

    if (gba->bus->game_rom[0xB2] != 0x96 || gba->bus->game_rom[0xB3] != 0x00)
        return false;

    for (int i = 0xB5; i < 0xBC; i++)
        if (gba->bus->game_rom[i] != 0x00)
            return false;

    uint8_t checksum = 0;
    for (int i = 0xA0; i < 0xBC; i++)
        checksum -= gba->bus->game_rom[i];
    checksum -= 0x19;

    if (gba->bus->game_rom[0xBD] != checksum) {
        eprintf("Invalid cartridge header checksum");
        return false;
    }

    if (gba->bus->game_rom[0xBE] != 0x00 && gba->bus->game_rom[0xBF] != 0x00)
        return false;

    // TODO multiboot entries

    return true;
}

static uint32_t read_open_bus(gba_t *gba, uint32_t address) {
    return 0xFFFFFFFF;
}

static void *bus_access(gba_t *gba, uint32_t address) {
    LOG_DEBUG("bus addr select: 0x%08X\n", address);

    gba_bus_t *bus = gba->bus;

    switch (address) {
    case BUS_BIOS_ROM ... BUS_BIOS_ROM_UNUSED - 1:
        bus->is_writeable = false;
        return &bus->bios_rom[address - BUS_BIOS_ROM];
    case BUS_WRAM_BOARD ... BUS_WRAM_BOARD_UNUSED - 1:
        bus->is_writeable = true;
        return &bus->wram_board[address - BUS_WRAM_BOARD];
    case BUS_WRAM_CHIP ... BUS_WRAM_CHIP_UNUSED - 1:
        bus->is_writeable = true;
        return &bus->wram_chip[address - BUS_WRAM_CHIP];
    case BUS_IO_REGS ... BUS_IO_REGS_UNUSED - 1:
        bus->is_writeable = true;
        return &bus->io_regs[address - BUS_IO_REGS];
    case BUS_PALETTE_RAM ... BUS_PALETTE_RAM_UNUSED - 1:
        bus->is_writeable = true;
        return &bus->palette_ram[address - BUS_PALETTE_RAM];
    case BUS_VRAM ... BUS_VRAM_UNUSED - 1:
        bus->is_writeable = true;
        return &bus->vram[address - BUS_VRAM];
    case BUS_OAM ... BUS_OAM_UNUSED - 1:
        bus->is_writeable = true;
        return &bus->oam[address - BUS_OAM];
    case BUS_GAME_ROM0 ... BUS_GAME_ROM1 - 1:
        bus->is_writeable = false;
        return &bus->game_rom[address - BUS_GAME_ROM0];
    case BUS_GAME_ROM1 ... BUS_GAME_ROM2 - 1:
        bus->is_writeable = false;
        return &bus->game_rom[address - BUS_GAME_ROM0];
    case BUS_GAME_ROM2 ... BUS_GAME_SRAM - 1:
        bus->is_writeable = false;
        return &bus->game_rom[address - BUS_GAME_ROM0];
    case BUS_GAME_SRAM ... BUS_GAME_UNUSED - 1:
        bus->is_writeable = true;
        return &bus->game_sram[address - BUS_GAME_SRAM];
    default:
        bus->is_writeable = false;
        return NULL;
    }
}

uint8_t gba_bus_read_byte(gba_t *gba, uint32_t address) {
    uint8_t *ret = bus_access(gba, ALIGN_ADDRESS(address, sizeof(*ret)));
    return ret ? *ret : read_open_bus(gba, address);
}

uint16_t gba_bus_read_half(gba_t *gba, uint32_t address) {
    uint16_t *ret = bus_access(gba, ALIGN_ADDRESS(address, sizeof(*ret)));
    return ret ? *ret : read_open_bus(gba, address);
}

uint32_t gba_bus_read_word(gba_t *gba, uint32_t address) {
    uint32_t *ret = bus_access(gba, ALIGN_ADDRESS(address, sizeof(*ret)));
    return ret ? *ret : read_open_bus(gba, address);
}

void gba_bus_write_byte(gba_t *gba, uint32_t address, uint8_t data) {
    uint8_t *ret = bus_access(gba, ALIGN_ADDRESS(address, sizeof(*ret)));
    if (ret)
        *ret = data;
}

void gba_bus_write_half(gba_t *gba, uint32_t address, uint16_t data) {
    uint16_t *ret = bus_access(gba, ALIGN_ADDRESS(address, sizeof(*ret)));
    if (ret)
        *ret = data;
}

void gba_bus_write_word(gba_t *gba, uint32_t address, uint32_t data) {
    uint32_t *ret = bus_access(gba, ALIGN_ADDRESS(address, sizeof(*ret)));
    if (ret)
        *ret = data;
}

// TODO this shouldn't be responsible for cartridge loading and parsing (same for gb_mmu_t)
bool gba_bus_init(gba_t *gba, const uint8_t *rom, size_t rom_size) {
    if (!rom || rom_size < 0xBF)
        return false;

    gba->bus = xcalloc(1, sizeof(*gba->bus));
    memcpy(gba->bus->game_rom, rom, rom_size);

    if (!gba_parse_cartridge(gba)) {
        gba_bus_quit(gba->bus);
        return false;
    }

    // TODO do not load bios from hardcoded file path
    FILE *f = fopen("src/bootroms/gba/gba_bios.bin", "r");
    if (!f) {
        gba_bus_quit(gba->bus);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    fread(gba->bus->bios_rom, sz, 1, f);
    fclose(f);

    return true;
}

void gba_bus_quit(gba_bus_t *bus) {
    free(bus);
}
