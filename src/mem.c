#include <stdio.h>
#include <string.h>

#include "types.h"
#include "mem.h"
#include "cpu.h"

byte_t cartridge[8000000];
byte_t mem[0x10000];

// TODO: handle fopen fail
static void load_bios(void) {
    memset(mem, 0, sizeof(mem));

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

    // FILE *f = fopen("roms/bios.gb", "rb");
    // fread(mem, 0xFF, 1, f);
    // fclose(f);

    // this is the result of the registers after bios execution (comment if bios is used)
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
    load_bios();

    FILE *f = fopen(filepath, "rb");
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
    memcpy(&mem, &cartridge[0], VRAM);
}

byte_t mem_read(word_t address) {
    // if (address < ROM_BANKN) {
    //     // printf("    mem read in ROM_BANK0 (%02X)\n", address);
    // } else if (address < VRAM) {
    //     printf("    mem read in ROM_BANKN (%02X)\n", address);
    // } else if (address < ERAM) {
    //     printf("    mem read in VRAM (%02X)\n", address);
    // } else if (address < WRAM_BANK0) {
    //     printf("    mem read in ERAM (%02X)\n", address);
    // } else if (address < WRAM_BANKN) {
    //     printf("    mem read in WRAM_BANK0 (%02X)\n", address);
    // } else if (address < MIRROR) {
    //     printf("    mem read in WRAM_BANKN (%02X)\n", address);
    // } else if (address < OAM) {
    //     printf("    mem read in MIRROR (%02X)\n", address);
    // } else if (address < UNUSABLE) {
    //     printf("    mem read in OAM (%02X)\n", address);
    // } else if (address < IO) {
    //     printf("    mem read in UNUSABLE (%02X)\n", address);
    // } else if (address < HRAM) {
    //     printf("    mem read in IO (%02X)\n", address);
    // } else if (address < INTERRUPTS) {
    //     printf("    mem read in HRAM (%02X)\n", address);
    // } else if (address == INTERRUPTS) {
    //     printf("    mem read in INTERRUPTS (%02X)\n", address);
    // } else {
    //     printf("    mem read outside memory!\n");
    // }
    return mem[address];
}

// TODO emulate mirror memory
void mem_write(word_t address, byte_t data) {
    if (address == DIV) {
        mem[address] = 0;
    } else {
        mem[address] = data;
    }
    // if (address < ROM_BANKN) {
    //     printf("    mem write %02X in ROM_BANK0 (%02X)\n", data, address);
    // } else if (address < VRAM) {
    //     printf("    mem write %02X in ROM_BANKN (%02X)\n", data, address);
    // } else if (address < ERAM) {
    //     printf("    mem write %02X in VRAM (%02X)\n", data, address);
    // } else if (address < WRAM_BANK0) {
    //     printf("    mem write %02X in ERAM (%02X)\n", data, address);
    // } else if (address < WRAM_BANKN) {
    //     printf("    mem write %02X in WRAM_BANK0 (%02X)\n", data, address);
    // } else if (address < MIRROR) {
    //     printf("    mem write %02X in WRAM_BANKN (%02X)\n", data, address);
    // } else if (address < OAM) {
    //     printf("    mem write %02X in MIRROR (%02X)\n", data, address);
    // } else if (address < UNUSABLE) {
    //     printf("    mem write %02X in OAM (%02X)\n", data, address);
    // } else if (address < IO) {
    //     printf("    mem write %02X in UNUSABLE (%02X)\n", data, address);
    // } else if (address < HRAM) {
    //     printf("    mem write %02X in IO (%02X)\n", data, address);
    // } else if (address < INTERRUPTS) {
    //     printf("    mem write %02X in HRAM (%02X)\n", data, address);
    // } else if (address == INTERRUPTS) {
    //     printf("    mem write %02X in INTERRUPTS (%02X)\n", data, address);
    // } else {
    //     printf("    mem write outside memory!\n");
    // }
}
