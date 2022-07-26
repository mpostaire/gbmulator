#pragma once

#include <time.h>

#include "types.h"

typedef enum {
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

    KEY0 = 0xFF4C,
    KEY1 = 0xFF4D, // Prepare Speed Switch (CGB-only)

    VBK = 0xFF4F, // VRAM Bank (CGB-only)

    BANK = 0xFF50,

    HDMA1 = 0xFF51, // New DMA Source, High (CGB-only)
    HDMA2 = 0xFF52, // New DMA Source, Low (CGB-only)
    HDMA3 = 0xFF53, // New DMA Destination, High (CGB-only)
    HDMA4 = 0xFF54, // New DMA Destination, Low (CGB-only)
    HDMA5 = 0xFF55, // New DMA Length/Mode/Start (CGB-only)

    RP = 0xFF56, // Infrared Communication Port (CGB-only)

    BGPI = 0xFF68, // Background Palette Index (CGB-only)
    BGPD = 0xFF69, // Background Palette Data (CGB-only)
    OBPI = 0xFF6A, // Object Palette Index (CGB-only)
    OBPD = 0xFF6B, // Object Palette Data (CGB-only)

    OPRI = 0xFF6C, // Object Priority Mode (CGB-only)

    SVBK = 0xFF70, // WRAM Bank (CGB-only)

    PCM12 = 0xFF76, // PCM amplitudes 1 & 2 (CGB-only)
    PCM34 = 0xFF77, // PCM amplitudes 3 & 4 (CGB-only)

    HRAM = 0xFF80,
    IE = 0xFFFF // Interrupt Enable
} mem_map_t;

int mmu_init(emulator_t *emu, const byte_t *rom_data, size_t rom_size);

void mmu_quit(emulator_t *emu);

void mmu_step(emulator_t *emu, int cycles);

/**
 * only used in cpu instructions (opcode execution)
 */
byte_t mmu_read(emulator_t *emu, word_t address);

/**
 * only used in cpu instructions (opcode execution)
 */
void mmu_write(emulator_t *emu, word_t address, byte_t data);

inline void rtc_update(rtc_t *rtc) {
    time_t now = time(NULL);
    time_t elapsed = now - rtc->timestamp;
    rtc->timestamp = now;
    rtc->value_in_seconds += elapsed;
    time_t value_in_seconds = rtc->value_in_seconds;

    word_t d = value_in_seconds / 86400;
    value_in_seconds %= 86400;

    // day overflow
    if (d >= 0x0200) {
        SET_BIT(rtc->dh, 7);
        d %= 0x0200;
    }

    rtc->dh |= (d & 0x100) >> 8;
    rtc->dl = d & 0xFF;

    rtc->h = value_in_seconds / 3600;
    value_in_seconds %= 3600;

    rtc->m = value_in_seconds / 60;
    value_in_seconds %= 60;

    rtc->s = value_in_seconds;

    // if there was a day overflow, emulate the overflow on rtc->value_in_seconds
    if (CHECK_BIT(rtc->dh, 7))
        rtc->value_in_seconds = rtc->s + rtc->m * 60 + rtc->h * 3600 + d * 86400;
}
