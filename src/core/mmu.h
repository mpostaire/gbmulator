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
    MMU_ROM_BANK0 = 0x0000, // From cartridge, usually a fixed bank.
    MMU_ROM_BANKN = 0x4000, // From cartridge, switchable bank via MBC (if any).
    MMU_VRAM = 0x8000,      // Only bank 0 in Non-CGB mode. Switchable bank 0/1 in CGB mode.
    MMU_ERAM = 0xA000,
    MMU_WRAM_BANK0 = 0xC000,
    MMU_WRAM_BANKN = 0xD000, // Only bank 1 in Non-CGB mode. Switchable bank 1~7 in CGB mode.
    MMU_ECHO = 0xE000,
    MMU_OAM = 0xFE00,
    MMU_UNUSABLE = 0xFEA0,
    MMU_IO = 0xFF00,
    MMU_HRAM = 0xFF80,
    MMU_IE = 0xFFFF // Interrupt Enable
} gb_mmu_map_t;

typedef enum {
    IO_P1 = 0x00, // Joypad

    // Serial Data Transfer
    IO_SB = 0x01, // Serial transfer data
    IO_SC = 0x02, // Serial transfer control

    // Timer
    IO_DIV = 0x04,  // Divider Register
    IO_TIMA = 0x05, // Timer counter
    IO_TMA = 0x06,  // Timer Modulo
    IO_TAC = 0x07,  // Timer Control

    IO_IF = 0x0F, // Interrupt Flag

    // Sound
    IO_NR10 = 0x10, // Channel 1 Sweep register
    IO_NR11 = 0x11, // Channel 1 Sound Length/Wave Pattern Duty
    IO_NR12 = 0x12, // Channel 1 Volume Envelope
    IO_NR13 = 0x13, // Channel 1 Frequency lo data
    IO_NR14 = 0x14, // Channel 1 Frequency hi data
    IO_NR20 = 0x15, // Unused
    IO_NR21 = 0x16, // Channel 2 Sound Length/Wave Pattern Duty
    IO_NR22 = 0x17, // Channel 2 Volume Envelope
    IO_NR23 = 0x18, // Channel 2 Frequency lo data
    IO_NR24 = 0x19, // Channel 2 Frequency hi data
    IO_NR30 = 0x1A, // Channel 3 Sound on/off
    IO_NR31 = 0x1B, // Channel 3 Sound Length
    IO_NR32 = 0x1C, // Channel 3 Select output level
    IO_NR33 = 0x1D, // Channel 3 Frequency lo data
    IO_NR34 = 0x1E, // Channel 3 Frequency hi data
    IO_NR40 = 0x1F, // Unused
    IO_NR41 = 0x20, // Channel 4 Sound Length
    IO_NR42 = 0x21, // Channel 4 Volume Envelope
    IO_NR43 = 0x22, // Channel 4 Polynomial Counter
    IO_NR44 = 0x23, // Channel 4 Counter/consecutive; Initial
    IO_NR50 = 0x24, // Channel control / ON-OFF / Volume
    IO_NR51 = 0x25, // Selection of Sound output terminal
    IO_NR52 = 0x26, // Sound on/off

    IO_WAVE_RAM = 0x30, // Wave Pattern RAM

    // Pixel Processing Unit (PPU)
    IO_LCDC = 0x40, // LCD Control
    IO_STAT = 0x41, // LCD Status
    IO_SCY = 0x42,  // Scroll Y
    IO_SCX = 0x43,  // Scroll X
    IO_LY = 0x44,   // LCD Y-Coordinate
    IO_LYC = 0x45,  // LY Compare
    IO_DMA = 0x46,  // DMA Transfer and Start Address
    IO_BGP = 0x47,  // BG Palette Data
    IO_OBP0 = 0x48, // Object Palette 0 Data
    IO_OBP1 = 0x49, // Object Palette 1 Data
    IO_WY = 0x4A,   // Window Y Position
    IO_WX = 0x4B,   // Window X Position + 7

    IO_KEY0 = 0x4C,
    IO_KEY1 = 0x4D, // Prepare Speed Switch (CGB-only)

    IO_VBK = 0x4F, // VRAM Bank (CGB-only)

    IO_BANK = 0x50,

    IO_HDMA1 = 0x51, // New DMA Source, High (CGB-only)
    IO_HDMA2 = 0x52, // New DMA Source, Low (CGB-only)
    IO_HDMA3 = 0x53, // New DMA Destination, High (CGB-only)
    IO_HDMA4 = 0x54, // New DMA Destination, Low (CGB-only)
    IO_HDMA5 = 0x55, // New DMA Length/Mode/Start (CGB-only)

    IO_RP = 0x56, // Infrared Communication Port (CGB-only)

    IO_BGPI = 0x68, // Background Palette Index (CGB-only)
    IO_BGPD = 0x69, // Background Palette Data (CGB-only)
    IO_OBPI = 0x6A, // Object Palette Index (CGB-only)
    IO_OBPD = 0x6B, // Object Palette Data (CGB-only)

    IO_OPRI = 0x6C, // Object Priority Mode (CGB-only)

    IO_SVBK = 0x70, // WRAM Bank (CGB-only)

    IO_PCM12 = 0x76, // PCM amplitudes 1 & 2 (CGB-only)
    IO_PCM34 = 0x77 // PCM amplitudes 3 & 4 (CGB-only)
} gb_io_reg_map_t;

typedef enum {
    IO_SRC_CPU,
    IO_SRC_OAM_DMA,
    IO_SRC_GDMA_HDMA
} gb_io_source_t;

typedef struct {
    byte_t *dmg_boot_rom;
    byte_t *cgb_boot_rom;

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
        byte_t allow_hdma_block; // set to 1 while the current 0x10 bytes block of HDMA can be copied, else 0 (gb->mode == CGB is assumed)
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
#define mmu_read(gb, address) mmu_read_io_src((gb), (address), IO_SRC_CPU)

/**
 * like mmu_write_io_src but using IO_SRC_CPU as io_src
 */
#define mmu_write(gb, address, data) mmu_write_io_src((gb), (address), (data), IO_SRC_CPU)

SERIALIZE_FUNCTION_DECLS(mmu);
