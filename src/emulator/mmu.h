#pragma once

#include "types.h"
#include "serialize.h"

#define ROM_BANK_SIZE 0x4000
#define VRAM_BANK_SIZE 0x2000
#define ERAM_BANK_SIZE 0x2000
#define WRAM_BANK_SIZE 0x1000

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
    DIV_LSB = 0xFF03, // Lower byte of the Divider Register
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

typedef enum {
    ROM_ONLY,
    MBC1,
    MBC1M,
    MBC2,
    MBC3,
    MBC30,
    MBC5,
    MBC6,
    MBC7,
    HuC1
} mbc_type_t;

typedef enum {
    GDMA,
    HDMA
} hdma_type_t;

typedef struct {
    byte_t latched_s;
    byte_t latched_m;
    byte_t latched_h;
    byte_t latched_dl;
    byte_t latched_dh;

    byte_t s;
    byte_t m;
    byte_t h;
    byte_t dl;
    byte_t dh;

    byte_t enabled;
    byte_t reg; // rtc register
    byte_t latch;
    uint32_t rtc_cycles;
} rtc_t;

typedef struct {
    byte_t boot_finished;

    size_t rom_size;
    // TODO dynamic allocation of rom, vram, eram, wram, cram_bg and cram_obj
    byte_t rom[8400000];
    byte_t vram[2 * VRAM_BANK_SIZE]; // DMG: 1 bank / CGB: 2 banks of size 0x2000
    byte_t eram[16 * ERAM_BANK_SIZE]; // max 16 banks of size 0x2000
    byte_t wram[8 * WRAM_BANK_SIZE]; // DMG: 2 banks / CGB: 8 banks of size 0x1000 (bank 0 non switchable)
    byte_t oam[0xA0];
    byte_t io_registers[0x80];
    byte_t hram[0x7F];
    byte_t ie;
    byte_t cram_bg[0x40]; // color palette memory: 8 palettes * 4 colors per palette * 2 bytes per color = 64 bytes
    byte_t cram_obj[0x40]; // color palette memory: 8 palettes * 4 colors per palette * 2 bytes per color = 64 bytes

    struct {
        byte_t step;
        byte_t progress;
        byte_t hdma_ly;
        byte_t lock_cpu;
        byte_t type;
    } hdma;

    struct {
        // array of statuses of the initialization of the oam dma (this is an array to allow multiple initializations
        // at the same time; the last will eventually overwrite the previous oam dma)
        byte_t starting_statuses[2];
        byte_t starting_count; // holds the number of oam dma initializations
        s_word_t progress; // < 0 if oam dma not running, else [0, 159)
        word_t src_address;
    } oam_dma;

    // offset to add to address in vram when accessing the 0x8000-0x9FFF range (signed because it can be negative)
    int32_t vram_bank_addr_offset;
    // offset to add to address in wram when accessing the 0xD000-0xDFFF range (signed because it can be negative)
    int32_t wram_bankn_addr_offset;

    byte_t mbc;
    word_t rom_banks; // number of rom banks
    byte_t eram_banks; // number of eram banks
    // address in rom that is the start of the current ROM bank when accessing the 0x0000-0x3FFF range
    uint32_t rom_bank0_addr;
    // address in rom that is the start of the current ROM bank when accessing the 0x4000-0x7FFF range
    // (actually with an offset of -ROM_BANK_SIZE to avoid adding this offset to the address passed to the mmu_read() function)
    uint32_t rom_bankn_addr;
    // address in eram that is the start of the current ERAM bank when accessing the 0x8000-0x9FFF range
    uint32_t eram_bank_addr;

    // MBC registers
    byte_t ramg_reg; // 1 if eram is enabled, else 0
    // MBC1/MBC1M: bits 7-5: unused, bits 4-0 (MBC1M: bits 3-0): lower 5 (MBC1M: 4) bits of the ROM_BANKN number
    // (also used as a convenience variable in other MBCs to store the ROM_BANKN number)
    byte_t bank1_reg;
    // bits 7-2: unused, bits 1-0: upper 2 bits (bits 6-5) of the ROM_BANKN number
    // (also used as a convenience variable in other MBCs to store the ERAM_BANK number)
    byte_t bank2_reg;
    byte_t mode_reg; // bits 7-1: unused, bit 0: BANK2 mode
    byte_t rtc_mapped; // MBC3
    byte_t romb0_reg; // MBC5: lower ROM BANK register
    byte_t romb1_reg; // MBC5: upper ROM BANK register
    // MBC5: RAMB register is stored in bank2_reg

    byte_t has_eram;
    byte_t has_battery;
    byte_t has_rumble;
    byte_t has_rtc;

    rtc_t rtc;
} mmu_t;

int mmu_init(emulator_t *emu, const byte_t *rom_data, size_t rom_size);

void mmu_quit(emulator_t *emu);

void mmu_step(emulator_t *emu);

void mmu_rtc_step(emulator_t *emu);

/**
 * only used in cpu instructions (opcode execution)
 */
byte_t mmu_read(emulator_t *emu, word_t address);

/**
 * only used in cpu instructions (opcode execution)
 */
void mmu_write(emulator_t *emu, word_t address, byte_t data);

SERIALIZE_FUNCTION_DECLS(mmu);
