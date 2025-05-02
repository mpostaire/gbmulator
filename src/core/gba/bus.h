#pragma once

#include "gba.h"

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

typedef enum {
    // All comments for the following IO registers have this format: SIZE ACCESS NAME DESCRIPTION

    // LCD I/O Registers
    IO_DISPCNT = 0x000,   // 2    R/W  DISPCNT   LCD Control
    IO_GREENSWAP = 0x002, // 2    R/W  -         Undocumented - Green Swap
    IO_DISPSTAT = 0x004,  // 2    R/W  DISPSTAT  General LCD Status (STAT,LYC)
    IO_VCOUNT = 0x006,    // 2    R    VCOUNT    Vertical Counter (LY)
    IO_BG0CNT = 0x008,    // 2    R/W  BG0CNT    BG0 Control
    IO_BG1CNT = 0x00A,    // 2    R/W  BG1CNT    BG1 Control
    IO_BG2CNT = 0x00C,    // 2    R/W  BG2CNT    BG2 Control
    IO_BG3CNT = 0x00E,    // 2    R/W  BG3CNT    BG3 Control
    IO_BG0HOFS = 0x010,   // 2    W    BG0HOFS   BG0 X-Offset
    IO_BG0VOFS = 0x012,   // 2    W    BG0VOFS   BG0 Y-Offset
    IO_BG1HOFS = 0x014,   // 2    W    BG1HOFS   BG1 X-Offset
    IO_BG1VOFS = 0x016,   // 2    W    BG1VOFS   BG1 Y-Offset
    IO_BG2HOFS = 0x018,   // 2    W    BG2HOFS   BG2 X-Offset
    IO_BG2VOFS = 0x01A,   // 2    W    BG2VOFS   BG2 Y-Offset
    IO_BG3HOFS = 0x01C,   // 2    W    BG3HOFS   BG3 X-Offset
    IO_BG3VOFS = 0x01E,   // 2    W    BG3VOFS   BG3 Y-Offset
    IO_BG2PA = 0x020,     // 2    W    BG2PA     BG2 Rotation/Scaling Parameter A (dx)
    IO_BG2PB = 0x022,     // 2    W    BG2PB     BG2 Rotation/Scaling Parameter B (dmx)
    IO_BG2PC = 0x024,     // 2    W    BG2PC     BG2 Rotation/Scaling Parameter C (dy)
    IO_BG2PD = 0x026,     // 2    W    BG2PD     BG2 Rotation/Scaling Parameter D (dmy)
    IO_BG2X = 0x028,      // 4    W    BG2X      BG2 Reference Point X-Coordinate
    IO_BG2Y = 0x02C,      // 4    W    BG2Y      BG2 Reference Point Y-Coordinate
    IO_BG3PA = 0x030,     // 2    W    BG3PA     BG3 Rotation/Scaling Parameter A (dx)
    IO_BG3PB = 0x032,     // 2    W    BG3PB     BG3 Rotation/Scaling Parameter B (dmx)
    IO_BG3PC = 0x034,     // 2    W    BG3PC     BG3 Rotation/Scaling Parameter C (dy)
    IO_BG3PD = 0x036,     // 2    W    BG3PD     BG3 Rotation/Scaling Parameter D (dmy)
    IO_BG3X = 0x038,      // 4    W    BG3X      BG3 Reference Point X-Coordinate
    IO_BG3Y = 0x03C,      // 4    W    BG3Y      BG3 Reference Point Y-Coordinate
    IO_WIN0H = 0x040,     // 2    W    WIN0H     Window 0 Horizontal Dimensions
    IO_WIN1H = 0x042,     // 2    W    WIN1H     Window 1 Horizontal Dimensions
    IO_WIN0V = 0x044,     // 2    W    WIN0V     Window 0 Vertical Dimensions
    IO_WIN1V = 0x046,     // 2    W    WIN1V     Window 1 Vertical Dimensions
    IO_WININ = 0x048,     // 2    R/W  WININ     Inside of Window 0 and 1
    IO_WINOUT = 0x04A,    // 2    R/W  WINOUT    Inside of OBJ Window & Outside of Windows
    IO_MOSAIC = 0x04C,    // 2    W    MOSAIC    Mosaic Size
    IO_BLDCNT = 0x050,    // 2    R/W  BLDCNT    Color Special Effects Selection
    IO_BLDALPHA = 0x052,  // 2    R/W  BLDALPHA  Alpha Blending Coefficients
    IO_BLDY = 0x054,      // 2    W    BLDY      Brightness (Fade-In/Out) Coefficient

    // Sound Registers
    IO_SOUND1CNT_L = 0x060, //  2    R/W  SOUND1CNT_L Channel 1 Sweep register       (NR10)
    IO_SOUND1CNT_H = 0x062, //  2    R/W  SOUND1CNT_H Channel 1 Duty/Length/Envelope (NR11, NR12)
    IO_SOUND1CNT_X = 0x064, //  2    R/W  SOUND1CNT_X Channel 1 Frequency/Control    (NR13, NR14)
    IO_SOUND2CNT_L = 0x068, //  2    R/W  SOUND2CNT_L Channel 2 Duty/Length/Envelope (NR21, NR22)
    IO_SOUND2CNT_H = 0x06C, //  2    R/W  SOUND2CNT_H Channel 2 Frequency/Control    (NR23, NR24)
    IO_SOUND3CNT_L = 0x070, //  2    R/W  SOUND3CNT_L Channel 3 Stop/Wave RAM select (NR30)
    IO_SOUND3CNT_H = 0x072, //  2    R/W  SOUND3CNT_H Channel 3 Length/Volume        (NR31, NR32)
    IO_SOUND3CNT_X = 0x074, //  2    R/W  SOUND3CNT_X Channel 3 Frequency/Control    (NR33, NR34)
    IO_SOUND4CNT_L = 0x078, //  2    R/W  SOUND4CNT_L Channel 4 Length/Envelope      (NR41, NR42)
    IO_SOUND4CNT_H = 0x07C, //  2    R/W  SOUND4CNT_H Channel 4 Frequency/Control    (NR43, NR44)
    IO_SOUNDCNT_L = 0x080,  //  2    R/W  SOUNDCNT_L  Control Stereo/Volume/Enable   (NR50, NR51)
    IO_SOUNDCNT_H = 0x082,  //  2    R/W  SOUNDCNT_H  Control Mixing/DMA Control
    IO_SOUNDCNT_X = 0x084,  //  2    R/W  SOUNDCNT_X  Control Sound on/off           (NR52)
    IO_SOUNDBIAS = 0x088,   //  2    BIOS SOUNDBIAS   Sound PWM Control
    IO_WAVE_RAM = 0x090,    // 2x10h R/W  WAVE_RAM  Channel 3 Wave Pattern RAM (2 banks!!)
    IO_FIFO_A = 0x0A0,      //  4    W    FIFO_A    Channel A FIFO, Data 0-3
    IO_FIFO_B = 0x0A4,      //  4    W    FIFO_B    Channel B FIFO, Data 0-3

    // DMA Transfer Channels
    IO_DMA0SAD = 0x0B0,   // 4    W    DMA0SAD   DMA 0 Source Address
    IO_DMA0DAD = 0x0B4,   // 4    W    DMA0DAD   DMA 0 Destination Address
    IO_DMA0CNT_L = 0x0B8, // 2    W    DMA0CNT_L DMA 0 Word Count
    IO_DMA0CNT_H = 0x0BA, // 2    R/W  DMA0CNT_H DMA 0 Control
    IO_DMA1SAD = 0x0BC,   // 4    W    DMA1SAD   DMA 1 Source Address
    IO_DMA1DAD = 0x0C0,   // 4    W    DMA1DAD   DMA 1 Destination Address
    IO_DMA1CNT_L = 0x0C4, // 2    W    DMA1CNT_L DMA 1 Word Count
    IO_DMA1CNT_H = 0x0C6, // 2    R/W  DMA1CNT_H DMA 1 Control
    IO_DMA2SAD = 0x0C8,   // 4    W    DMA2SAD   DMA 2 Source Address
    IO_DMA2DAD = 0x0CC,   // 4    W    DMA2DAD   DMA 2 Destination Address
    IO_DMA2CNT_L = 0x0D0, // 2    W    DMA2CNT_L DMA 2 Word Count
    IO_DMA2CNT_H = 0x0D2, // 2    R/W  DMA2CNT_H DMA 2 Control
    IO_DMA3SAD = 0x0D4,   // 4    W    DMA3SAD   DMA 3 Source Address
    IO_DMA3DAD = 0x0D8,   // 4    W    DMA3DAD   DMA 3 Destination Address
    IO_DMA3CNT_L = 0x0DC, // 2    W    DMA3CNT_L DMA 3 Word Count
    IO_DMA3CNT_H = 0x0DE, // 2    R/W  DMA3CNT_H DMA 3 Control

    // Timer Registers
    IO_TM0CNT_L = 0x100, // 2    R/W  TM0CNT_L  Timer 0 Counter/Reload
    IO_TM0CNT_H = 0x102, // 2    R/W  TM0CNT_H  Timer 0 Control
    IO_TM1CNT_L = 0x104, // 2    R/W  TM1CNT_L  Timer 1 Counter/Reload
    IO_TM1CNT_H = 0x106, // 2    R/W  TM1CNT_H  Timer 1 Control
    IO_TM2CNT_L = 0x108, // 2    R/W  TM2CNT_L  Timer 2 Counter/Reload
    IO_TM2CNT_H = 0x10A, // 2    R/W  TM2CNT_H  Timer 2 Control
    IO_TM3CNT_L = 0x10C, // 2    R/W  TM3CNT_L  Timer 3 Counter/Reload
    IO_TM3CNT_H = 0x10E, // 2    R/W  TM3CNT_H  Timer 3 Control

    // Serial Communication (1)
    IO_SIODATA32 = 0x120,   // 4    R/W  SIODATA32   SIO Data (Normal-32bit Mode; shared with below)
    IO_SIOMULTI0 = 0x120,   // 2    R/W  SIOMULTI0   SIO Data 0 (Parent)    (Multi-Player Mode)
    IO_SIOMULTI1 = 0x122,   // 2    R/W  SIOMULTI1   SIO Data 1 (1st Child) (Multi-Player Mode)
    IO_SIOMULTI2 = 0x124,   // 2    R/W  SIOMULTI2   SIO Data 2 (2nd Child) (Multi-Player Mode)
    IO_SIOMULTI3 = 0x126,   // 2    R/W  SIOMULTI3   SIO Data 3 (3rd Child) (Multi-Player Mode)
    IO_SIOCNT = 0x128,      // 2    R/W  SIOCNT      SIO Control Register
    IO_SIOMLT_SEND = 0x12A, // 2    R/W  SIOMLT_SEND SIO Data (Local of MultiPlayer; shared below)
    IO_SIODATA8 = 0x12A,    // 2    R/W  SIODATA8    SIO Data (Normal-8bit and UART Mode)

    // Keypad Input
    IO_KEYINPUT = 0x130, // 2    R    KEYINPUT  Key Status
    IO_KEYCNT = 0x132,   // 2    R/W  KEYCNT    Key Interrupt Control

    // Serial Communication (2)
    IO_RCNT = 0x134,      // 2    R/W  RCNT      SIO Mode Select/General Purpose Data
    IO_IR = 0x136,        // -    -    IR        Ancient - Infrared Register (Prototypes only)
    IO_JOYCNT = 0x140,    // 2    R/W  JOYCNT    SIO JOY Bus Control
    IO_JOY_RECV = 0x150,  // 4    R/W  JOY_RECV  SIO JOY Bus Receive Data
    IO_JOY_TRANS = 0x154, // 4    R/W  JOY_TRANS SIO JOY Bus Transmit Data
    IO_JOYSTAT = 0x158,   // 2    R/?  JOYSTAT   SIO JOY Bus Receive Status

    // Interrupt, Waitstate, and Power-Down Control
    IO_IE = 0x200,      // 2    R/W  IE        Interrupt Enable Register
    IO_IF = 0x202,      // 2    R/W  IF        Interrupt Request Flags / IRQ Acknowledge
    IO_WAITCNT = 0x204, // 2    R/W  WAITCNT   Game Pak Waitstate Control
    IO_IME = 0x208,     // 2    R/W  IME       Interrupt Master Enable Register
    IO_POSTFLG = 0x300, // 1    R/W  POSTFLG   Undocumented - Post Boot Flag
    IO_HALTCNT = 0x301, // 1    W    HALTCNT   Undocumented - Power Down Control
    // 0x410  ?    ?    ?         Undocumented - Purpose Unknown / Bug ??? 0FFh
    // 0x800  4    R/W  ?         Undocumented - Internal Memory Control (R/W)
    // 0x800  4    R/W  ?         Mirrors of 4000800h (repeated each 64K)
    // 0x000  4    W    (3DS)     Disable ARM7 bootrom overlay (3DS only)
} gba_io_reg_map_t;

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

    size_t rom_size;
} gba_bus_t;

uint8_t gba_bus_read_byte(gba_t *gba, uint32_t address);

uint16_t gba_bus_read_half(gba_t *gba, uint32_t address);

uint32_t gba_bus_read_word(gba_t *gba, uint32_t address);

void gba_bus_write_byte(gba_t *gba, uint32_t address, uint8_t data);

void gba_bus_write_half(gba_t *gba, uint32_t address, uint16_t data);

void gba_bus_write_word(gba_t *gba, uint32_t address, uint32_t data);

bool gba_bus_init(gba_t *gba, const uint8_t *rom, size_t rom_size);

void gba_bus_quit(gba_bus_t *bus);
