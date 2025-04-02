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

void gba_bus_select(gba_t *gba, uint32_t address) {
    LOG_DEBUG("bus addr select: 0x%08X\n", address);

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
        LOG_DEBUG("bus address 0x%08X is read only", gba->bus->address);
        return;
    } else if (!gba->bus->selected_mem_ptr) {
        LOG_DEBUG("bus address 0x%08X is unmapped memory", gba->bus->address);
        return;
    }

    LOG_DEBUG("bus write of 0x%08X at 0x%08X", data, gba->bus->address);

    // TODO special cases like io registers
    gba->bus->selected_mem_ptr[gba->bus->selected_mem_offset] = data;
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
