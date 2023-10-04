#pragma once

#include "types.h"
#include "serialize.h"
#include "mbc.h"

#define ROM_BANK_SIZE 0x4000
#define VRAM_BANK_SIZE 0x2000
#define ERAM_BANK_SIZE 0x2000
#define WRAM_BANK_SIZE 0x1000
#define CRAM_BG_SIZE 0x0040
#define CRAM_OBJ_SIZE 0x0040

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
} gb_memory_map_t;

typedef enum {
    IO_SRC_CPU,
    IO_SRC_OAM_DMA,
    IO_SRC_GDMA_HDMA
} gb_io_source_t;

typedef struct {
    size_t rom_size;

    byte_t *rom; // max size: 8400000
    byte_t vram[2 * VRAM_BANK_SIZE]; // DMG: 1 bank / CGB: 2 banks of size 0x2000
    byte_t eram[16 * ERAM_BANK_SIZE]; // max 16 banks of size 0x2000
    byte_t wram[8 * WRAM_BANK_SIZE]; // DMG: 2 banks / CGB: 8 banks of size 0x1000 (bank 0 non switchable)
    byte_t oam[0xA0];
    byte_t io_registers[0x80];
    byte_t hram[0x7F];
    byte_t ie;
    byte_t cram_bg[CRAM_BG_SIZE]; // color palette memory: 8 palettes * 4 colors per palette * 2 bytes per color = 64 bytes
    byte_t cram_obj[CRAM_OBJ_SIZE]; // color palette memory: 8 palettes * 4 colors per palette * 2 bytes per color = 64 bytes

    byte_t boot_finished;

    struct {
        byte_t initializing; // 4 cycles of initialization delay
        byte_t allow_hdma_block; // set to 1 while the current 0x10 bytes block of HDMA can be copied, else 0 (emu->mode == CGB is assumed)
        byte_t lock_cpu;
        byte_t type;
        byte_t progress;
        word_t src_address;
        word_t dest_address;
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

    word_t rom_banks; // number of rom banks
    byte_t eram_banks; // number of eram banks
    // address in rom that is the start of the current ROM bank when accessing the 0x0000-0x3FFF range
    uint32_t rom_bank0_addr;
    // address in rom that is the start of the current ROM bank when accessing the 0x4000-0x7FFF range
    // (actually with an offset of -ROM_BANK_SIZE to avoid adding this offset to the address passed to the mmu_read() function)
    uint32_t rom_bankn_addr;
    // address in eram that is the start of the current ERAM bank when accessing the 0x8000-0x9FFF range
    uint32_t eram_bank_addr;

    byte_t has_battery;
    byte_t has_rumble;
    byte_t has_rtc;

    gb_mbc_t mbc;
} gb_mmu_t;

int parse_header_mbc_byte(byte_t mbc_byte, byte_t *mbc_type, byte_t *has_eram, byte_t *has_battery, byte_t *has_rtc, byte_t *has_rumble);

int validate_header_checksum(const byte_t *rom);

int mmu_init(gb_t *gb, const byte_t *rom, size_t rom_size);

void mmu_quit(gb_t *gb);

byte_t mmu_read_io_src(gb_t *gb, word_t address, gb_io_source_t io_src);

void mmu_write_io_src(gb_t *gb, word_t address, byte_t data, gb_io_source_t io_src);

/**
 * like mmu_read_io_src but using IO_SRC_CPU as io_src
 */
#define mmu_read(emu, address) mmu_read_io_src((emu), (address), IO_SRC_CPU)

/**
 * like mmu_write_io_src but using IO_SRC_CPU as io_src
 */
#define mmu_write(emu, address, data) mmu_write_io_src((emu), (address), (data), IO_SRC_CPU)

SERIALIZE_FUNCTION_DECLS(mmu);
