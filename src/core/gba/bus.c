#include <stdlib.h>

#include "gba_priv.h"

#define ALIGN_ADDRESS(address, n) ((address) & (~((n) - 1))) // TODO maybe wrong

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
    if (gba->bus->is_writeable)
        printf("bus address 0x%08X is read only\n", data, gba->bus->address);
    else
        printf("bus write of 0x%08X ignored at 0x%08X\n", data, gba->bus->address);
}

gba_bus_t *gba_bus_init(uint8_t *rom, size_t rom_size, uint8_t *bios, size_t bios_size) {
    if (!gba_is_rom_valid(rom, rom_size))
        return NULL;

    gba_bus_t *bus = xcalloc(1, sizeof(*bus));
    memcpy(bus->game_rom, rom, rom_size);
    memcpy(bus->bios_rom, bios, bios_size);
    return bus;
}

void gba_bus_quit(gba_bus_t *bus) {
    free(bus);
}
