#pragma once

#include "types.h"

extern byte_t cartridge[8000000];
extern byte_t mem[0x10000];

enum mem_map {
    ROM_BANK0 = 0x0000,
    ROM_BANKN = 0x4000,
    VRAM = 0x8000,
    ERAM = 0xA000,
    WRAM_BANK0 = 0xC000,
    WRAM_BANKN = 0xD000,
    MIRROR = 0xE000,
    OAM = 0xFE00,
    UNUSABLE = 0xFEA0,
    IO = 0xFF00,
    JOYP = 0xFF00, // JOYPAD
    DIV = 0xFF04, // Divider Register
    TIMA = 0xFF05, // Timer counter
    TMA = 0xFF06, // Timer Modulo
    TAC = 0xFF07, // Timer Control
    IF = 0xFF0F, // Interrupt Flag
    LCDC = 0xFF40, // LCD Control
    LY = 0xFF44, // LCD Y coordinate
    HRAM = 0xFF80,
    IE = 0xFFFF // Interrupt Enable
};

void load_cartridge(char *filepath);

/**
 * only used in instructions (opcode execution)
 */
byte_t mem_read(word_t address);

/**
 * only used in instructions (opcode execution)
 */
void mem_write(word_t address, byte_t data);
