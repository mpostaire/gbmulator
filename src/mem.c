#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "types.h"
#include "mem.h"
#include "cpu.h"
#include "ppu.h"
#include "joypad.h"

enum mbc_type {
    MBC0,
    MBC1,
    MBC2,
    MBC3,
    MBC5 = 5,
    MBC6,
    MBC7
};

byte_t cartridge[8000000];
byte_t mem[0x10000];
byte_t mbc;
byte_t rom_banks;
byte_t ram_banks;
byte_t eram_enabled = 0;
byte_t current_rom_bank = 1;
byte_t current_ram_bank = 0;
byte_t mbc_mode = 0;

static void load_bios(void) {
    FILE *f = fopen("roms/bios.gb", "rb");
    if (f) {
        fread(mem, 0x100, 1, f);
        fclose(f);
        return;
    }

    // this is the state of the memory after bios execution
    mem[0xFF05] = 0x00;
    mem[0xFF06] = 0x00;
    mem[0xFF07] = 0x00;
    mem[0xFF10] = 0x80;
    mem[0xFF11] = 0xBF;
    mem[0xFF12] = 0xF3;
    mem[0xFF14] = 0xBF;
    mem[0xFF16] = 0x3F;
    mem[0xFF17] = 0x00;
    mem[0xFF19] = 0xBF;
    mem[0xFF1A] = 0x7F;
    mem[0xFF1B] = 0xFF;
    mem[0xFF1C] = 0x9F;
    mem[0xFF1E] = 0xBF;
    mem[0xFF20] = 0xFF;
    mem[0xFF21] = 0x00;
    mem[0xFF22] = 0x00;
    mem[0xFF23] = 0xBF;
    mem[0xFF24] = 0x77;
    mem[0xFF25] = 0xF3;
    mem[0xFF26] = 0xF1;
    mem[0xFF40] = 0x91;
    mem[0xFF42] = 0x00;
    mem[0xFF43] = 0x00;
    mem[0xFF45] = 0x00;
    mem[0xFF47] = 0xFC;
    mem[0xFF48] = 0xFF;
    mem[0xFF49] = 0xFF;
    mem[0xFF4A] = 0x00;
    mem[0xFF4B] = 0x00;
    mem[0xFFFF] = 0x00;

    // this is the state of the registers after bios execution
    registers.a = 0x01;
    registers.f = 0xB0;
    registers.b = 0x00;
    registers.c = 0x13;
    registers.d = 0x00;
    registers.e = 0xD8;
    registers.h = 0x01;
    registers.l = 0x4D;
    registers.pc = 0x100;
    registers.sp = 0xFFFE;
}

void load_cartridge(char *filepath) {
    // clear memory
    memset(&mem, 0, sizeof(mem));

    FILE *f = fopen(filepath, "rb");
    if (!f) {
        perror("load_cartridge");
        exit(EXIT_FAILURE);
    }
    fread(cartridge, sizeof(cartridge), 1, f);

    fclose(f);

    if (cartridge[0x0143] == 0xC0) {
        printf("CGB only rom!\n");
        exit(EXIT_FAILURE);
    }

    switch (cartridge[0x0147]) {
    case 0x00:
        mbc = MBC0;
        break;
    case 0x01: case 0x02: case 0x03:
        mbc = MBC1;
        break;
    case 0x05: case 0x06:
        mbc = MBC2;
        break;
    case 0x0F: case 0x10: case 0x11: case 0x12: case 0x13:
        mbc = MBC3;
        break;
    case 0x19: case 0x1A: case 0x1B: case 0x1C: case 0x1D: case 0x1E:
        mbc = MBC5;
        break;
    case 0x20:
        mbc = MBC6;
        break;
    case 0x22:
        mbc = MBC7;
        break;
    default:
        // TODO handle this case
        break;
    }

    rom_banks = 2 << cartridge[0x0148];

    switch (cartridge[0x0149]) {
    case 0x02: ram_banks = 1; break;
    case 0x03: ram_banks = 4; break;
    case 0x04: ram_banks = 16; break;
    case 0x05: ram_banks = 8; break;
    default:
        // TODO handle this case
        break;
    }

    char title[15];
    strncpy(title, (char *) &cartridge[0x134], 15);
    title[15] = '\0';
    printf("playing %s\n", title);
    printf("using MBC%d --> %d ROM banks + %d RAM banks\n", mbc, rom_banks, ram_banks);

    // load cartridge into rom banks (everything before VRAM (0x8000))
    memcpy(&mem, &cartridge, VRAM);

    // Load bios (if it exists) after cartridge to overwrite first 0x100 bytes.
    // If bios don't exist, sets memory and registers accordingly. 
    load_bios();
}

// TODO MBC1 special cases where rom bank 0 is changed are not implemented
static void handle_mbc_registers(word_t address, byte_t data) {
    if (mbc == MBC0)
        return; // read only, do nothing
    
    if (mbc == MBC1) {
        if (address < 0x2000) {
            // FIXME > launch with bits_ramg.gb and see output message
            eram_enabled = (data & 0x0F) == 0x0A;
        } else if (address < 0x4000) {
            current_rom_bank = data & 0x1F;
            if (!current_rom_bank)
                current_rom_bank = 0x01; // 0x00 not allowed
        } else if (address < 0x6000) {
            current_ram_bank = data & 0x03;
            if (!mbc_mode) { // ROM mode
                current_rom_bank = ((data & 0x03) << 5) | (data & 0x1F);
                current_ram_bank = 0;
            }
        } else if (address < 0x8000) {
            mbc_mode = data & 0x01;
        }
    }
}

byte_t mem_read(word_t address) {
    if (mbc == MBC1) {
        if (address >= 0x4000 && address < 0x8000) // ROM_BANKN
            return cartridge[(address - 0x4000) + (current_rom_bank * 0x4000)];

        if (address >= 0xA000 && address < 0xC000 && eram_enabled) // ERAM
            return cartridge[(address - 0xA000) + (current_ram_bank * 0x2000)];
    }

    // OAM inaccessible by cpu while ppu in mode 2 or 3 and LCD is enabled (return undefined data)
    if (address >= OAM && address < UNUSABLE && CHECK_BIT(mem[LCDC], 7) && ((PPU_IS_MODE(PPU_OAM) || PPU_IS_MODE(PPU_DRAWING))))
        return 0xFF;

    // VRAM inaccessible by cpu while ppu in mode 3 and LCD is enabled (return undefined data)
    if ((address >= VRAM && address < ERAM) && CHECK_BIT(mem[LCDC], 7) && PPU_IS_MODE(PPU_DRAWING))
        return 0xFF;

    // UNUSABLE memory is unusable
    if (address >= UNUSABLE && address < IO)
        return 0xFF;
    
    // Reading from P1 register returns joypad input state according to its current bit 4 or 5 value
    if (address == P1)
        return joypad_get_input();

    return mem[address];
}

void mem_write(word_t address, byte_t data) {
    if (address < VRAM) {
        handle_mbc_registers(address, data);
    } else if (mbc == MBC1 && address >= ERAM && address < WRAM_BANK0 && eram_enabled) {
        cartridge[(address - 0xA000) + (current_ram_bank * 0x2000)] = data;
    } else if (address == DIV) {
        // writing to DIV resets it to 0
        mem[address] = 0;
    } else if ((address >= OAM && address < UNUSABLE) && ((PPU_IS_MODE(PPU_OAM) || PPU_IS_MODE(PPU_DRAWING)) && CHECK_BIT(mem[LCDC], 7))) {
        // OAM inaccessible by cpu while ppu in mode 2 or 3 and LCD enabled
    // } else if ((address >= VRAM && address < ERAM) && (PPU_IS_MODE(PPU_DRAWING) && CHECK_BIT(mem[LCDC], 7))) {
        // FIXME this condition commented right now because it causes display errors (missing characters) in test roms
        // VRAM inaccessible by cpu while ppu in mode 3 and LCD enabled
    } else if (address >= UNUSABLE && address < IO) {
        // UNUSABLE memory is unusable
    } else if (address == LY) {
        // read only
    } else if (address == DMA) {
        // OAM DMA transfer
        // TODO this should not be instantaneous (it takes 640 cycles to complete and during that time the cpu can only access HRAM)
        memcpy(&mem[OAM], &mem[data * 0x100], 0xA0 * sizeof(byte_t));
        mem[address] = data;
    } else if (address == 0xFF50 && data == 1) {
        // disable boot rom
        memcpy(&mem, &cartridge, 0x100);
        mem[address] = data;
    } else if (address == P1) {
        // prevent writes to the lower nibble of the P1 register (joypad)
        mem[address] = data & 0xF0;
    } else {
        mem[address] = data;
    }
}
