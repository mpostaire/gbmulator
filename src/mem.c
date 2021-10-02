#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "types.h"
#include "mem.h"
#include "cpu.h"

byte_t cartridge[8000000];
byte_t mem[0x10000];

// TODO: handle fopen fail
static void load_bios(void) {
    FILE *f = fopen("roms/bios.gb", "rb");
    if (f) {
        fread(mem, 0xFF, 1, f);
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

    // switch (cartridge[0x147]) {
    //     case 1: MBC1 = 1; break;
    //     case 2: MBC1 = 1; break;
    //     case 3: MBC1 = 1; break;
    //     case 5: MBC2 = 1; break;
    //     case 6: MBC2 = 1; break;

    //     default: break;
    // }

    // load cartridge into rom banks (everything before VRAM (0x8000))
    memcpy(&mem, &cartridge, VRAM);

    // Load bios (if it exists) after cartridge to overwrite first 0x100 bytes.
    // If bios don't exist, sets memory and registers accordingly. 
    load_bios();
}

// TODO MBC
byte_t mem_read(word_t address) {
    // OAM inaccessible by cpu while ppu in mode 2 or 3 and LCD is enabled (return undefined data)
    if (address >= OAM && address < UNUSABLE && CHECK_BIT(mem[LCDC], 7) && ((mem[STAT] & 0x3) == 2 || (mem[STAT] & 0x3) == 3))
        return 0xFF;

    // VRAM inaccessible by cpu while ppu in mode 3 and LCD is enabled (return undefined data)
    if ((address >= VRAM && address < ERAM) && CHECK_BIT(mem[LCDC], 7) && (mem[STAT] & 0x3) == 3)
        return 0xFF;

    // UNUSABLE memory is unusable
    if (address >= UNUSABLE && address < IO)
        return 0xFF;

    return mem[address];
}

void mem_write(word_t address, byte_t data) {
    // TODO this is supposed to print test roms debug messages
    if (address == SC && data == 0x81) {
        printf("%c", mem[SB]);
    }

    if (address <= VRAM) {
        // TODO rom banks
        // printf("/!\\ ROM BANKING NOT IMPLEMENTED YET! /!\\\n");
        mem[address] = data; // should this write data in memory anyway?
    } else if (address == DIV) {
        // writing to DIV resets it to 0
        mem[address] = 0;
    } else if (address >= OAM && address < UNUSABLE && ((mem[STAT] & 0x3) == 2 || (mem[STAT] & 0x3) == 3) && CHECK_BIT(mem[LCDC], 7)) {
        // OAM inaccessible by cpu while ppu in mode 2 or 3 and LCD enabled
        return;
    } else if ((address >= VRAM && address < ERAM) && (mem[STAT] & 0x3) == 3 && CHECK_BIT(mem[LCDC], 7)) {
        // VRAM inaccessible by cpu while ppu in mode 3 and LCD enabled
        return;
    } else if (address >= UNUSABLE && address < IO) {
        // UNUSABLE memory is unusable
        return;
    } else if (address == LY) {
        // read only
        return;
    } else if (address == DMA) {
        // OAM DMA transfer
        // TODO this should not be instantaneous (it takes 640 cycles to complete and during that time the cpu can only access HRAM)
        memcpy(&mem[OAM], &mem[data * 0x100], 0xA0 * sizeof(byte_t));
        mem[address] = data;
    } else if (address == 0xFF50 && data == 1) {
        // disable boot rom
        memcpy(&mem, &cartridge, 0x100);
        mem[address] = data;
    } else {
        mem[address] = data;
    }
}
