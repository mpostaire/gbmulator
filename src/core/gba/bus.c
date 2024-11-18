#include <stdlib.h>

#include "gba_priv.h"

// TODO there are multiple buses (1 for each diff memory regions? maybe some memory regions use same bus?)
void gba_bus_select(gba_t *gba, uint32_t address) {
    printf("bus addr select: 0x%08X\n", address);

    gba->bus->address = address;

    // switch (address & 0x0) {
    // case 0x0:
    //     break;
    // }

    if (address >= BUS_IO_REGS && address < BUS_IO_REGS_UNUSED)
        return;

    if (address >= BUS_BIOS_ROM_UNUSED) {
        eprintf("cannot access data at address 0x%08X", gba->bus->address);
        exit(42);
    }
}

// uint8_t bus_read_byte(gba_t *gba) {
//     gba_bus_t *bus = gba->bus;
//     // TODO needs converting address to corresponding bus memory region using the bus memory map
//     return bus->bios_rom[bus->address];
// }

#define ALIGN_ADDRESS(address, n) ((address) & (~((n) - 1))) // TODO maybe wrong
uint8_t gba_bus_read_byte(gba_t *gba) {
    uint8_t ret;
    uint32_t address = ALIGN_ADDRESS(gba->bus->address, sizeof(ret));
    memcpy(&ret, &gba->bus->bios_rom[address], sizeof(ret));
    return ret;
}
uint16_t gba_bus_read_half(gba_t *gba) {
    uint16_t ret;
    uint32_t address = ALIGN_ADDRESS(gba->bus->address, sizeof(ret));
    memcpy(&ret, &gba->bus->bios_rom[address], sizeof(ret));
    return ret;
}
uint32_t gba_bus_read_word(gba_t *gba) {
    uint32_t ret;
    uint32_t address = ALIGN_ADDRESS(gba->bus->address, sizeof(ret));
    memcpy(&ret, &gba->bus->bios_rom[address], sizeof(ret));
    return ret;
}

// TODO data type may be wrong
void gba_bus_write(gba_t *gba, uint32_t data) {
    // TODO needs converting address to corresponding bus memory region using the bus memory map

    // switch (gba->bus->address) {
    // case 0x0:
    //     break;
    // }

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
