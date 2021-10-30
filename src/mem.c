#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "types.h"
#include "mem.h"
#include "cpu.h"
#include "ppu.h"
#include "joypad.h"
#include "apu.h"

enum mbc_type {
    MBC0,
    MBC1,
    MBC2,
    MBC3,
    MBC5,
    MBC6,
    MBC7
};

struct rtc_counter {
    byte_t s;
    byte_t m;
    byte_t h;
    byte_t dl;
    byte_t dh;
    time_t value_in_seconds; // current rtc counter seconds, minutes, hours and days in seconds (helps with rtc_update())
};

byte_t cartridge[8000000];
byte_t mem[0x10000];
byte_t eram[0x8000]; // max 4 banks of size 0x2000

byte_t mbc;
byte_t rom_banks;
byte_t ram_banks;
byte_t current_rom_bank = 1;
byte_t current_eram_bank = 0;
byte_t mbc1_mode = 0;
byte_t eram_enabled = 0;
byte_t rtc_enabled = 0;
byte_t rtc_register = 0;
byte_t rtc_latch = 0;
struct rtc_counter rtc;

char *rom_filepath;

static char *get_save_filepath(void) {
    size_t len = strlen(rom_filepath);
    char *buf = malloc(len + 2);
    if (!buf) {
        perror("ERROR get_save_filepath");
        exit(EXIT_FAILURE);
    }
    strncpy(buf, rom_filepath, len);
    for (int i = len + 1; i >= 0; i--) {
        if (buf[i] == '.') {
            buf[i + 1] = 's';
            buf[i + 2] = 'a';
            buf[i + 3] = 'v';
            break;
        }
    }
    buf[len + 1] = '\0';
    return buf;
}

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
    rom_filepath = filepath;

    // clear memory
    memset(&mem, 0, sizeof(mem));
    // clear eram
    memset(&eram, 0, sizeof(eram));

    FILE *f = fopen(filepath, "rb");
    if (!f) {
        perror("ERROR: load_cartridge");
        exit(EXIT_FAILURE);
    }
    fread(cartridge, sizeof(cartridge), 1, f);

    fclose(f);

    if (cartridge[0x0143] == 0xC0) {
        printf("ERROR: CGB only rom\n");
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
    // case 0x19: case 0x1A: case 0x1B: case 0x1C: case 0x1D: case 0x1E:
    //     mbc = MBC5;
    //     break;
    // case 0x20:
    //     mbc = MBC6;
    //     break;
    // case 0x22:
    //     mbc = MBC7;
    //     break;
    default:
        printf("ERROR: MBC byte %02X not supported\n", cartridge[0x0147]);
        exit(EXIT_FAILURE);
        break;
    }

    rom_banks = 2 << cartridge[0x0148];

    switch (cartridge[0x0149]) {
    case 0x00: ram_banks = 0; break;
    case 0x02: ram_banks = 1; break;
    case 0x03: ram_banks = 4; break;
    case 0x04: ram_banks = 16; break;
    case 0x05: ram_banks = 8; break;
    default:
        // TODO handle this case
        break;
    }

    // get rom title
    char title[16];
    strncpy(title, (char *) &cartridge[0x134], 15);
    title[15] = '\0';
    printf("Playing %s\n", title);
    printf("Cartridge using MBC%d with %d ROM banks + %d RAM banks\n", mbc, rom_banks, ram_banks);

    // checksum validation
    int sum = 0;
    for (int i = 0x0134; i <= 0x014C; i++)
        sum = sum - cartridge[i] - 1;
    if (((byte_t) (sum & 0xFF)) != cartridge[0x014D]) {
        printf("ERROR: invalid checksum\n");
        exit(EXIT_FAILURE);
    }

    // load cartridge into rom banks (everything before VRAM (0x8000))
    memcpy(&mem, &cartridge, VRAM);

    // Load bios (if it exists) after cartridge to overwrite first 0x100 bytes.
    // If bios don't exist, sets memory and registers accordingly. 
    load_bios();

    // load save into ERAM
    char *save_filepath = get_save_filepath();
    f = fopen(save_filepath, "rb");
    free(save_filepath);
    if (!f) return; // if can't read save file, ignore it and proceed without save.
    fread(eram, sizeof(eram), 1, f);
    fclose(f);
}

void mem_save_eram(void) {
    if (!ram_banks) return;

    char *save_filepath = get_save_filepath();
    FILE *f = fopen(save_filepath, "wb");
    free(save_filepath);

    if (!f) {
        perror("ERROR opening the save file");
        exit(EXIT_FAILURE);
    }
    if (!fwrite(eram, sizeof(eram), 1, f)) {
        printf("ERROR writing to save file\n");
        exit(EXIT_FAILURE);
    }
    fclose(f);
}

static void rtc_update(void) {
    // get current time in seconds
    time_t current_update_time = time(NULL);

    // get time elapsed
    time_t elapsed = current_update_time - rtc.value_in_seconds;

    rtc.value_in_seconds += elapsed;

    word_t d = rtc.value_in_seconds / 86400;
    elapsed %= 86400;
    if (d >= 0x0200) { // day overflow
        SET_BIT(rtc.dh, 7);
        d %= 0x0200;
    }
    rtc.dh |= (d & 0x01) >> 8;
    rtc.dl = d & 0xFF;

    rtc.h = elapsed / 3600;
    elapsed %= 3600;

    rtc.m = elapsed / 60;
    elapsed %= 60;

    rtc.s = elapsed;
}

// TODO MBC1 special cases where rom bank 0 is changed are not implemented
static void write_mbc_registers(word_t address, byte_t data) {
    switch (mbc) {
    case MBC0:
        break; // read only, do nothing
    case MBC1: // TODO not all mooneye's MBC tests pass
        if (address < 0x2000) {
            eram_enabled = (data & 0x0F) == 0x0A;
        } else if (address < 0x4000) {
            current_rom_bank = data & 0x1F;
            if (current_rom_bank == 0x00)
                current_rom_bank++;
            current_rom_bank %= rom_banks;
        } else if (address < 0x6000) {
            if (mbc1_mode) { // ROM mode
                current_eram_bank = data & 0x03;
                current_eram_bank %= ram_banks;
            } else {
                current_rom_bank = ((data & 0x03) << 5) | (current_rom_bank & 0x1F);
                if (current_rom_bank == 0x00 || current_rom_bank == 0x20 || current_rom_bank == 0x40 || current_rom_bank == 0x60)
                    current_rom_bank++;
                current_rom_bank %= rom_banks;
                current_eram_bank = 0;
            }
        } else if (address < 0x8000) {
            mbc1_mode = data & 0x01;
        }
        break;
    case MBC2: // TODO not all mooneye's MBC tests pass
        if (address < 0x2000) {
            if (!CHECK_BIT(address, 8))
                eram_enabled = (data & 0x0F) == 0x0A;
        } else if (address < 0x4000) {
            current_rom_bank = data & 0x0F;
            if (current_rom_bank == 0x00)
                current_rom_bank = 0x01; // 0x00 not allowed
            current_rom_bank %= rom_banks;
        }
        break;
    case MBC3:
        if (address < 0x2000) {
            eram_enabled = (data & 0x0F) == 0x0A;
            rtc_enabled = (data & 0x0F) == 0x0A;
        } else if (address < 0x4000) {
            current_rom_bank = data & 0x7F;
            if (current_rom_bank == 0x00)
                current_rom_bank = 0x01; // 0x00 not allowed
            current_rom_bank %= rom_banks;
        } else if (address < 0x6000) {
            if (data <= 0x03) {
                current_eram_bank = data;
                current_eram_bank %= ram_banks;
            } else if (data >= 0x08 && data <= 0x0C) {
                rtc_register = data;
                current_eram_bank = -1;
            }
        } else if (address < 0x8000) {
            if (rtc_latch == 0x00 && data == 0x01 && !CHECK_BIT(rtc.dh, 6))
                rtc_update();
            rtc_latch = data;
        }
        break;
    default:
        break;
    }
}

// TODO not all mooneye's MBC tests pass
static void write_mbc_eram(word_t address, byte_t data) {
    switch (mbc) {
    case MBC1:
        if (eram_enabled)
            eram[(address - 0xA000) + (current_eram_bank * 0x2000)] = data;
        break;
    case MBC2:
        if (eram_enabled)
            eram[(address - 0xA000) + (current_eram_bank * 0x2000)] = data & 0x0F;
        break;
    case MBC3:
        if (eram_enabled && current_eram_bank >= 0) {
            eram[(address - 0xA000) + (current_eram_bank * 0x2000)] = data;
        } else if (rtc_enabled) {
            switch (rtc_register) {
            case 0x08: rtc.s = data; break;
            case 0x09: rtc.m = data; break;
            case 0x0A: rtc.h = data; break;
            case 0x0B: rtc.dl = data; break;
            case 0x0C: rtc.dh = data; break;
            default: break;
            }
        }
        break;
    default:
        break;
    }
}

byte_t mem_read(word_t address) {
    if (mbc == MBC1 || mbc == MBC2 || mbc == MBC3) { // TODO not all mooneye's MBC tests pass
        if (address >= 0x4000 && address < 0x8000) // ROM_BANKN
            return cartridge[(address - 0x4000) + (current_rom_bank * 0x4000)];

        if (address >= 0xA000 && address < 0xC000) { // ERAM
            if (rtc_enabled) { // implies mbc == MBC3 because rtc_enabled is set to 0 by default
                switch (rtc_register) {
                case 0x08: return rtc.s;
                case 0x09: return rtc.m;
                case 0x0A: return rtc.h;
                case 0x0B: return rtc.dl;
                case 0x0C: return rtc.dh;
                default: break; // break, not return because we want to access eram (if enabled) in this case
                }
            }

            return eram_enabled ? eram[(address - 0xA000) + (current_eram_bank * 0x2000)] : 0xFF;
        }
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
    
    if (address == NR10)
        return mem[NR10] | 0x80;
    if (address == NR11)
        return mem[NR11] | 0x3F;
    if (address == NR12)
        return mem[NR12] | 0x00; // useless?
    if (address == NR13)
        return 0xFF;
    if (address == NR14)
        return mem[NR14] | 0xBF;

    if (address == NR20)
        return 0xFF;
    if (address == NR21)
        return mem[NR21] | 0x3F;
    if (address == NR22)
        return mem[NR22] | 0x00; // useless?
    if (address == NR23)
        return 0xFF;
    if (address == NR24)
        return mem[NR24] | 0xBF;

    if (address == NR30)
        return (channel3.dac_enabled << 7) | 0x7F;
    if (address == NR31)
        return 0xFF;
    if (address == NR32)
        return mem[NR32] | 0x9F;
    if (address == NR33)
        return 0xFF;
    if (address == NR34)
        return mem[NR34] | 0xBF;

    if (address == NR40)
        return 0xFF;
    if (address == NR41)
        return 0xFF;
    if (address == NR42)
        return mem[NR42] | 0x00; // useless?
    if (address == NR43)
        return mem[NR43] | 0x00; // useless?
    if (address == NR44)
        return mem[NR44] | 0xBF;

    if (address == NR50)
        return mem[NR50] | 0x00; // useless?
    if (address == NR51)
        return mem[NR51] | 0x00; // useless?
    if (address == NR52) // TODO fix buggy channel enables (01-registers in blargg dmg sound tests)
        return (apu_enabled << 7) | 0x70 | channel4.enabled << 3 | channel3.enabled << 2 | channel2.enabled << 1 | channel1.enabled;
    if (address > NR52 && address < WAVE_RAM)
        return 0xFF;

    return mem[address];
}

void mem_write(word_t address, byte_t data) {
    if (address < VRAM) {
        write_mbc_registers(address, data);
    } else if (address >= ERAM && address < WRAM_BANK0) {
        write_mbc_eram(address, data);
    } else if (address == DIV) {
        // writing to DIV resets it to 0
        mem[address] = 0;
    } else if ((address >= OAM && address < UNUSABLE) && ((PPU_IS_MODE(PPU_OAM) || PPU_IS_MODE(PPU_DRAWING)) && CHECK_BIT(mem[LCDC], 7))) {
        // OAM inaccessible by cpu while ppu in mode 2 or 3 and LCD enabled
    // } else if ((address >= VRAM && address < ERAM) && (PPU_IS_MODE(PPU_DRAWING) && CHECK_BIT(mem[LCDC], 7))) {
        // FIXME this condition commented right now because it causes display errors
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
    } else if (address == NR10) {
        if (!apu_enabled)
            return;
        mem[address] = data;
    } else if (address == NR11) {
        if (!apu_enabled)
            return;
        mem[address] = data;
        channel1.length_timer = 64 - (data & 0x3F);
    } else if (address == NR12) {
        if (!apu_enabled)
            return;
        mem[address] = data;
        channel1.dac_enabled = (data >> 3) != 0;
        if (!channel1.dac_enabled)
            channel1.enabled = 0;
    } else if (address == NR13) {
        if (!apu_enabled)
            return;
        mem[address] = data;
    } else if (address == NR14) {
        if (!apu_enabled)
            return;
        mem[address] = data;
        if (CHECK_BIT(data, 7) && channel1.dac_enabled)
            apu_channel_trigger(&channel1);
    } else if (address == NR20) {
        if (!apu_enabled)
            return;
        mem[address] = data;
    } else if (address == NR21) {
        if (!apu_enabled)
            return;
        mem[address] = data;
        channel2.length_timer = 64 - (data & 0x3F);
    } else if (address == NR22) {
        if (!apu_enabled)
            return;
        mem[address] = data;
        channel2.dac_enabled = (data >> 3) != 0;
        if (!channel2.dac_enabled)
            channel2.enabled = 0;
    } else if (address == NR23) {
        if (!apu_enabled)
            return;
        mem[address] = data;
    } else if (address == NR24) {
        if (!apu_enabled)
            return;
        mem[address] = data;
        if (CHECK_BIT(data, 7) && channel2.dac_enabled)
            apu_channel_trigger(&channel2);
    } else if (address == NR30) {
        if (!apu_enabled)
            return;
        mem[address] = data;
        channel3.dac_enabled = (data >> 7) != 0;
        if (!channel3.dac_enabled)
            channel3.enabled = 0;
    } else if (address == NR31) {
        if (!apu_enabled)
            return;
        mem[address] = data;
        channel3.length_timer = 256 - data;
    } else if (address == NR32) {
        if (!apu_enabled)
            return;
        mem[address] = data;
    } else if (address == NR33) {
        if (!apu_enabled)
            return;
        mem[address] = data;
    } else if (address == NR34) {
        if (!apu_enabled)
            return;
        mem[address] = data;
        if (CHECK_BIT(data, 7) && channel3.dac_enabled)
            apu_channel_trigger(&channel3);
    } else if (address == NR40) {
        if (!apu_enabled)
            return;
        mem[address] = data;
    } else if (address == NR41) {
        if (!apu_enabled)
            return;
        mem[address] = data;
        channel4.length_timer = 64 - (data & 0x3F);
    } else if (address == NR42) {
        if (!apu_enabled)
            return;
        mem[address] = data;
        channel4.dac_enabled = (data >> 3) != 0;
        if (!channel4.dac_enabled)
            channel4.enabled = 0;
    } else if (address == NR43) {
        if (!apu_enabled)
            return;
        mem[address] = data;
    } else if (address == NR44) {
        if (!apu_enabled)
            return;
        mem[address] = data;
        if (CHECK_BIT(data, 7) && channel4.dac_enabled)
            apu_channel_trigger(&channel4);
    } else if (address == NR50) {
        if (!apu_enabled)
            return;
        mem[address] = data;
    } else if (address == NR51) {
        if (!apu_enabled)
            return;
        mem[address] = data;
    } else if (address == NR52) {
        apu_enabled = CHECK_BIT(data, 7) ? 1 : 0;
        if (!CHECK_BIT(data, 7)) {
            mem[NR10] = 0x00;
            mem[NR11] = 0x00;
            mem[NR12] = 0x00;
            mem[NR13] = 0x00;
            mem[NR14] = 0x00;

            mem[NR21] = 0x00;
            mem[NR22] = 0x00;
            mem[NR23] = 0x00;
            mem[NR24] = 0x00;

            mem[NR30] = 0x00;
            mem[NR31] = 0x00;
            mem[NR32] = 0x00;
            mem[NR33] = 0x00;
            mem[NR34] = 0x00;

            mem[NR41] = 0x00;
            mem[NR42] = 0x00;
            mem[NR43] = 0x00;
            mem[NR44] = 0x00;

            mem[NR50] = 0x00;
            mem[NR51] = 0x00;
        }
    } else if (address >= WAVE_RAM && address < LCDC) {
        if (!CHECK_BIT(mem[NR30], 7))
            mem[address] = data;
    } else {
        mem[address] = data;
    }
}
