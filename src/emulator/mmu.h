#pragma once

#include "types.h"

typedef struct {
    byte_t s;
    byte_t m;
    byte_t h;
    byte_t dl;
    byte_t dh;
    time_t value_in_seconds; // current rtc counter seconds, minutes, hours and days in seconds (helps with rtc_update())
    byte_t enabled;
    byte_t reg; // rtc register
    byte_t latch;
} rtc_t;

typedef struct {
    const char *rom_filepath;
    const char *save_filepath;
    char rom_title[17];

    byte_t cartridge[8000000];
    // do not move the 'mem' member (savestate.c uses offsetof mem on this struct)
    // everything that is below this line will be saved in the savestates
    byte_t mem[0x10000];
    byte_t eram[0x8000]; // max 4 banks of size 0x2000

    byte_t mbc;
    byte_t rom_banks;
    byte_t ram_banks;
    byte_t current_rom_bank;
    s_word_t current_eram_bank;
    byte_t mbc1_mode;
    byte_t eram_enabled;
    rtc_t rtc;
} mmu_t;

enum mem_map {
    ROM_BANK0 = 0x0000, // From cartridge, usually a fixed bank.
    ROM_BANKN = 0x4000, // From cartridge, switchable bank via MBC (if any).
    VRAM = 0x8000,      // Only bank 0 in Non-CGB mode. Switchable bank 0/1 in CGB mode.
    ERAM = 0xA000,
    WRAM_BANK0 = 0xC000,
    WRAM_BANKN = 0xD000, // Only bank 1 in Non-CGB mode. Switchable bank 1~7 in CGB mode.
    ECHO = 0xE000,
    OAM = 0xFE00,
    UNUSABLE = 0xFEA0,
    IO = 0xFF00,
    P1 = 0xFF00, // Joypad

    // Serial Data Transfer
    SB = 0xFF01, // Serial transfer data
    SC = 0xFF02, // Serial transfer control

    // Timer
    DIV = 0xFF04,  // Divider Register
    TIMA = 0xFF05, // Timer counter
    TMA = 0xFF06,  // Timer Modulo
    TAC = 0xFF07,  // Timer Control

    IF = 0xFF0F, // Interrupt Flag

    // Sound
    NR10 = 0xFF10, // Channel 1 Sweep register
    NR11 = 0xFF11, // Channel 1 Sound Length/Wave Pattern Duty
    NR12 = 0xFF12, // Channel 1 Volume Envelope
    NR13 = 0xFF13, // Channel 1 Frequency lo data
    NR14 = 0xFF14, // Channel 1 Frequency hi data
    NR20 = 0xFF15, // Unused
    NR21 = 0xFF16, // Channel 2 Sound Length/Wave Pattern Duty
    NR22 = 0xFF17, // Channel 2 Volume Envelope
    NR23 = 0xFF18, // Channel 2 Frequency lo data
    NR24 = 0xFF19, // Channel 2 Frequency hi data
    NR30 = 0xFF1A, // Channel 3 Sound on/off
    NR31 = 0xFF1B, // Channel 3 Sound Length
    NR32 = 0xFF1C, // Channel 3 Select output level
    NR33 = 0xFF1D, // Channel 3 Frequency lo data
    NR34 = 0xFF1E, // Channel 3 Frequency hi data
    NR40 = 0xFF1F, // Unused
    NR41 = 0xFF20, // Channel 4 Sound Length
    NR42 = 0xFF21, // Channel 4 Volume Envelope
    NR43 = 0xFF22, // Channel 4 Polynomial Counter
    NR44 = 0xFF23, // Channel 4 Counter/consecutive; Initial
    NR50 = 0xFF24, // Channel control / ON-OFF / Volume
    NR51 = 0xFF25, // Selection of Sound output terminal
    NR52 = 0xFF26, // Sound on/off

    WAVE_RAM = 0xFF30, // Wave Pattern RAM

    // Pixel Processing Unit (PPU)
    LCDC = 0xFF40, // LCD Control
    STAT = 0xFF41, // LCDC Status
    SCY = 0xFF42,  // Scroll Y
    SCX = 0xFF43,  // Scroll X
    LY = 0xFF44,   // LCD Y-Coordinate
    LYC = 0xFF45,  // LY Compare
    DMA = 0xFF46,  // DMA Transfer and Start Address
    BGP = 0xFF47,  // BG Palette Data
    OBP0 = 0xFF48, // Object Palette 0 Data
    OBP1 = 0xFF49, // Object Palette 1 Data
    WY = 0xFF4A,   // Window Y Position
    WX = 0xFF4B,   // Window X Position + 7

    BANK = 0xFF50,

    HRAM = 0xFF80,
    IE = 0xFFFF // Interrupt Enable
};

enum mbc_type {
    MBC0,
    MBC1,
    MBC2,
    MBC3,
    MBC5,
    MBC6,
    MBC7
};

extern mmu_t mmu;

void mmu_init(const char *save_path);

void mmu_load_cartridge(const char *filepath);

void mmu_save_eram(void);

/**
 * only used in instructions (opcode execution)
 */
byte_t mmu_read(word_t address);

/**
 * only used in instructions (opcode execution)
 */
void mmu_write(word_t address, byte_t data);
