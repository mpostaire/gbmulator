#pragma once

#include "types.h"
#include "../utils.h"

typedef enum {
    // General Internal Memory
    BUS_BIOS_ROM = 0x00000000,          // BIOS - System ROM (16 KBytes)
    BUS_BIOS_ROM_UNUSED = 0x00004000,   // Not used
    BUS_WRAM_BOARD = 0x02000000,        // WRAM - On-board Work RAM (256 KBytes) 2 Wait
    BUS_WRAM_BOARD_UNUSED = 0x02040000, // Not used
    BUS_WRAM_CHIP = 0x03000000,         // WRAM - On-chip Work RAM (32 KBytes)
    BUS_WRAM_CHIP_UNUSED = 0x03008000,  // Not used
    BUS_IO_REGS = 0x04000000,           // I/O Registers
    BUS_IO_REGS_UNUSED = 0x04000400,    // Not used

    // Internal Display Memory
    BUS_PALETTE_RAM = 0x05000000,        // BG/OBJ Palette RAM (1 Kbyte)
    BUS_PALETTE_RAM_UNUSED = 0x05000400, // Not used
    BUS_VRAM = 0x06000000,               // VRAM - Video RAM (96 KBytes)
    BUS_VRAM_UNUSED = 0x06018000,        // Not used
    BUS_OAM = 0x07000000,                // OAM - OBJ Attributes (1 Kbyte)
    BUS_OAM_UNUSED = 0x07000400,         // Not used

    // External Memory (Game Pak)
    BUS_GAME_ROM0 = 0x08000000,   // Game Pak ROM/FlashROM (max 32MB) - Wait State 0
    BUS_GAME_ROM1 = 0x0A000000,   // Game Pak ROM/FlashROM (max 32MB) - Wait State 1
    BUS_GAME_ROM2 = 0x0C000000,   // Game Pak ROM/FlashROM (max 32MB) - Wait State 2
    BUS_GAME_SRAM = 0x0E000000,   // Game Pak SRAM (max 64 KBytes) - 8bit Bus width
    BUS_GAME_UNUSED = 0x0E010000, // Not used

    BUS_UNUSED = 0x10000000 // Not used (upper 4bits of address bus unused)
} gba_bus_map_t;

typedef struct {
    uint8_t bios_rom[BUS_BIOS_ROM_UNUSED - BUS_BIOS_ROM];
    uint8_t wram_board[BUS_WRAM_BOARD_UNUSED - BUS_WRAM_BOARD];
    uint8_t wram_chip[BUS_WRAM_CHIP_UNUSED - BUS_WRAM_CHIP];
    uint8_t io_regs[BUS_IO_REGS_UNUSED - BUS_IO_REGS];
    uint8_t palette_ram[BUS_PALETTE_RAM_UNUSED - BUS_PALETTE_RAM];
    uint8_t vram[BUS_VRAM_UNUSED - BUS_VRAM];
    uint8_t oam[BUS_OAM_UNUSED - BUS_OAM];
    uint8_t game_rom[BUS_GAME_ROM1 - BUS_GAME_ROM0];
    uint8_t game_sram[BUS_GAME_UNUSED - BUS_GAME_SRAM];

    uint32_t address; // current selected address on the bus
} gba_bus_t;

void gba_bus_select(gba_t *gba, uint32_t address);

uint8_t gba_bus_read_byte(gba_t *gba);

uint16_t gba_bus_read_half(gba_t *gba);

uint32_t gba_bus_read_word(gba_t *gba);

// TODO data type may be wrong
void gba_bus_write(gba_t *gba, uint32_t data);

gba_bus_t *gba_bus_init(uint8_t *rom, size_t rom_size, uint8_t *bios, size_t bios_size);

void gba_bus_quit(gba_bus_t *bus);
