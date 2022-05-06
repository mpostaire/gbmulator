#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "types.h"
#include "mmu.h"
#include "utils.h"
#include "ppu.h"
#include "joypad.h"
#include "apu.h"
#include "boot.h"

mmu_t mmu;

static void load_cartridge(const char *filepath) {
    mmu.rom_filepath = filepath;

    // clear memory
    memset(&mmu.mem, 0, sizeof(mmu.mem));
    // clear eram
    memset(&mmu.eram, 0, sizeof(mmu.eram));

    FILE *f = fopen(filepath, "rb");
    if (!f) {
        errnoprintf("opening file");
        exit(EXIT_FAILURE);
    }
    fread(mmu.cartridge, sizeof(mmu.cartridge), 1, f);

    fclose(f);

    if (mmu.cartridge[0x0143] == 0xC0) {
        eprintf("CGB only rom: this emulator does not support CGB games yet\n");
        exit(EXIT_FAILURE);
    }

    switch (mmu.cartridge[0x0147]) {
    case 0x00:
        mmu.mbc = MBC0;
        break;
    case 0x01: case 0x02: case 0x03:
        mmu.mbc = MBC1;
        break;
    case 0x05: case 0x06:
        mmu.mbc = MBC2;
        break;
    case 0x0F: case 0x10: case 0x11: case 0x12: case 0x13:
        mmu.mbc = MBC3;
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
        eprintf("MBC byte %02X not supported\n", mmu.cartridge[0x0147]);
        exit(EXIT_FAILURE);
        break;
    }

    mmu.rom_banks = 2 << mmu.cartridge[0x0148];

    switch (mmu.cartridge[0x0149]) {
    case 0x00: mmu.ram_banks = 0; break;
    case 0x02: mmu.ram_banks = 1; break;
    case 0x03: mmu.ram_banks = 4; break;
    case 0x04: mmu.ram_banks = 16; break;
    case 0x05: mmu.ram_banks = 8; break;
    default:
        // TODO handle this case
        break;
    }

    printf("Cartridge using MBC%d with %d ROM banks + %d RAM banks\n", mmu.mbc, mmu.rom_banks, mmu.ram_banks);

    // get rom title
    memcpy(&mmu.rom_title, (char *) &mmu.cartridge[0x134], 16);
    mmu.rom_title[16] = '\0';

    // checksum validation
    int sum = 0;
    for (int i = 0x0134; i <= 0x014C; i++)
        sum = sum - mmu.cartridge[i] - 1;
    if (((byte_t) (sum & 0xFF)) != mmu.cartridge[0x014D]) {
        eprintf("invalid checksum\n");
        exit(EXIT_FAILURE);
    }

    // load cartridge into rom banks (everything before VRAM (0x8000))
    memcpy(&mmu.mem, &mmu.cartridge, VRAM);

    // Load bios after cartridge to overwrite first 0x100 bytes.
    memcpy(&mmu.mem, &dmg_boot, sizeof(dmg_boot));

    // load save into ERAM
    f = fopen(mmu.save_filepath, "rb");
    // if there is a save file, load it into eram
    if (f) {
        fread(mmu.eram, sizeof(mmu.eram), 1, f);
        fclose(f);
    }
}

void mmu_init(const char *rom_path, const char *save_path) {
    mmu = (mmu_t) {
        .save_filepath = save_path,
        .current_rom_bank = 1
    };
    load_cartridge(rom_path);
}

void mmu_save_eram(void) {
    if (!mmu.ram_banks) return;

    FILE *f = fopen(mmu.save_filepath, "wb");
    if (!f) {
        errnoprintf("opening the save file");
        exit(EXIT_FAILURE);
    }
    if (!fwrite(mmu.eram, sizeof(mmu.eram), 1, f)) {
        eprintf("writing to save file\n");
        fclose(f);
        exit(EXIT_FAILURE);
    }
    fclose(f);
}

static void rtc_update(void) {
    // get current time in seconds
    time_t current_update_time = time(NULL);

    // get time elapsed
    time_t elapsed = current_update_time - mmu.rtc.value_in_seconds;

    mmu.rtc.value_in_seconds += elapsed;

    word_t d = mmu.rtc.value_in_seconds / 86400;
    elapsed %= 86400;
    if (d >= 0x0200) { // day overflow
        SET_BIT(mmu.rtc.dh, 7);
        d %= 0x0200;
    }
    mmu.rtc.dh |= (d & 0x01) >> 8;
    mmu.rtc.dl = d & 0xFF;

    mmu.rtc.h = elapsed / 3600;
    elapsed %= 3600;

    mmu.rtc.m = elapsed / 60;
    elapsed %= 60;

    mmu.rtc.s = elapsed;
}

// TODO MBC1 special cases where rom bank 0 is changed are not implemented
// TODO rewrite mbc to use true registers
static void write_mbc_registers(word_t address, byte_t data) {
    switch (mmu.mbc) {
    case MBC0:
        break; // read only, do nothing
    case MBC1: // TODO not all mooneye's MBC tests pass
        if (address < 0x2000) {
            mmu.eram_enabled = (data & 0x0F) == 0x0A;
        } else if (address < 0x4000) {
            mmu.current_rom_bank = data & 0x1F;
            if (mmu.current_rom_bank == 0x00)
                mmu.current_rom_bank = 0x01;
            mmu.current_rom_bank &= mmu.rom_banks - 1; // in this case, equivalent to current_rom_bank %= rom_banks but avoid division by 0
        } else if (address < 0x6000) {
            if (mmu.mbc1_mode) { // ROM mode
                mmu.current_eram_bank = data & 0x03;
                mmu.current_eram_bank &= mmu.ram_banks - 1; // in this case, equivalent to current_eram_bank %= ram_banks but avoid division by 0
            } else {
                mmu.current_rom_bank = ((data & 0x03) << 5) | (mmu.current_rom_bank & 0x1F);
                if (mmu.current_rom_bank == 0x00 || mmu.current_rom_bank == 0x20 || mmu.current_rom_bank == 0x40 || mmu.current_rom_bank == 0x60)
                    mmu.current_rom_bank++;
                mmu.current_rom_bank &= mmu.rom_banks - 1; // in this case, equivalent to current_rom_bank %= rom_banks but avoid division by 0
                mmu.current_eram_bank = 0x00;
            }
        } else if (address < 0x8000) {
            mmu.mbc1_mode = data & 0x01;
        }
        break;
    case MBC2: // TODO not all mooneye's MBC tests pass
        if (address < 0x2000) {
            if (!CHECK_BIT(address, 8))
                mmu.eram_enabled = (data & 0x0F) == 0x0A;
        } else if (address < 0x4000) {
            mmu.current_rom_bank = data & 0x0F;
            if (mmu.current_rom_bank == 0x00)
                mmu.current_rom_bank = 0x01; // 0x00 not allowed
            mmu.current_rom_bank &= mmu.rom_banks - 1; // in this case, equivalent to current_rom_bank %= rom_banks but avoid division by 0
        }
        break;
    case MBC3:
        if (address < 0x2000) {
            mmu.eram_enabled = (data & 0x0F) == 0x0A;
            mmu.rtc.enabled = (data & 0x0F) == 0x0A;
        } else if (address < 0x4000) {
            mmu.current_rom_bank = data & 0x7F;
            if (mmu.current_rom_bank == 0x00)
                mmu.current_rom_bank = 0x01; // 0x00 not allowed
            mmu.current_rom_bank &= mmu.rom_banks - 1; // in this case, equivalent to current_rom_bank %= rom_banks but avoid division by 0
        } else if (address < 0x6000) {
            if (data <= 0x03) {
                mmu.current_eram_bank = data;
                mmu.current_eram_bank &= mmu.ram_banks - 1; // in this case, equivalent to current_eram_bank %= ram_banks but avoid division by 0
            } else if (data >= 0x08 && data <= 0x0C) {
                mmu.rtc.reg = data;
                mmu.current_eram_bank = -1;
            }
        } else if (address < 0x8000) {
            if (mmu.rtc.latch == 0x00 && data == 0x01 && !CHECK_BIT(mmu.rtc.dh, 6))
                rtc_update();
            mmu.rtc.latch = data;
        }
        break;
    default:
        break;
    }
}

// TODO not all mooneye's MBC tests pass
static void write_mbc_eram(word_t address, byte_t data) {
    switch (mmu.mbc) {
    case MBC1:
        if (mmu.eram_enabled)
            mmu.eram[(address - 0xA000) + (mmu.current_eram_bank * 0x2000)] = data;
        break;
    case MBC2:
        if (mmu.eram_enabled)
            mmu.eram[(address - 0xA000) + (mmu.current_eram_bank * 0x2000)] = data & 0x0F;
        break;
    case MBC3:
        if (mmu.eram_enabled && mmu.current_eram_bank >= 0) {
            mmu.eram[(address - 0xA000) + (mmu.current_eram_bank * 0x2000)] = data;
        } else if (mmu.rtc.enabled) {
            switch (mmu.rtc.reg) {
            case 0x08: mmu.rtc.s = data; break;
            case 0x09: mmu.rtc.m = data; break;
            case 0x0A: mmu.rtc.h = data; break;
            case 0x0B: mmu.rtc.dl = data; break;
            case 0x0C: mmu.rtc.dh = data; break;
            default: break;
            }
        }
        break;
    default:
        break;
    }
}

byte_t mmu_read(word_t address) {
    if (mmu.mbc == MBC1 || mmu.mbc == MBC2 || mmu.mbc == MBC3) { // TODO not all mooneye's MBC tests pass
        if (address >= 0x4000 && address < 0x8000) // ROM_BANKN
            return mmu.cartridge[(address - 0x4000) + (mmu.current_rom_bank * 0x4000)];

        if (address >= 0xA000 && address < 0xC000) { // ERAM
            if (mmu.rtc.enabled) { // implies mbc == MBC3 because rtc_enabled is set to 0 by default
                switch (mmu.rtc.reg) {
                case 0x08: return mmu.rtc.s;
                case 0x09: return mmu.rtc.m;
                case 0x0A: return mmu.rtc.h;
                case 0x0B: return mmu.rtc.dl;
                case 0x0C: return mmu.rtc.dh;
                default: break; // break, not return because we want to access eram (if enabled) in this case
                }
            }

            return mmu.eram_enabled ? mmu.eram[(address - 0xA000) + (mmu.current_eram_bank * 0x2000)] : 0xFF;
        }
    }

    // OAM inaccessible by cpu while ppu in mode 2 or 3 and LCD is enabled (return undefined data)
    if (address >= OAM && address < UNUSABLE && CHECK_BIT(mmu.mem[LCDC], 7) && ((PPU_IS_MODE(PPU_OAM) || PPU_IS_MODE(PPU_DRAWING))))
        return 0xFF;

    // VRAM inaccessible by cpu while ppu in mode 3 and LCD is enabled (return undefined data)
    if ((address >= VRAM && address < ERAM) && CHECK_BIT(mmu.mem[LCDC], 7) && PPU_IS_MODE(PPU_DRAWING))
        return 0xFF;

    // UNUSABLE memory is unusable
    if (address >= UNUSABLE && address < IO)
        return 0xFF;
    
    // Reading from P1 register returns joypad input state according to its current bit 4 or 5 value
    if (address == P1)
        return joypad_get_input();
    
    if (address == NR10)
        return mmu.mem[NR10] | 0x80;
    if (address == NR11)
        return mmu.mem[NR11] | 0x3F;
    if (address == NR12)
        return mmu.mem[NR12] | 0x00; // useless?
    if (address == NR13)
        return 0xFF;
    if (address == NR14)
        return mmu.mem[NR14] | 0xBF;

    if (address == NR20)
        return 0xFF;
    if (address == NR21)
        return mmu.mem[NR21] | 0x3F;
    if (address == NR22)
        return mmu.mem[NR22] | 0x00; // useless?
    if (address == NR23)
        return 0xFF;
    if (address == NR24)
        return mmu.mem[NR24] | 0xBF;

    if (address == NR30)
        return mmu.mem[NR30] | 0x7F;
    if (address == NR31)
        return 0xFF;
    if (address == NR32)
        return mmu.mem[NR32] | 0x9F;
    if (address == NR33)
        return 0xFF;
    if (address == NR34)
        return mmu.mem[NR34] | 0xBF;

    if (address == NR40)
        return 0xFF;
    if (address == NR41)
        return 0xFF;
    if (address == NR42)
        return mmu.mem[NR42] | 0x00; // useless?
    if (address == NR43)
        return mmu.mem[NR43] | 0x00; // useless?
    if (address == NR44)
        return mmu.mem[NR44] | 0xBF;

    if (address == NR50)
        return mmu.mem[NR50] | 0x00; // useless?
    if (address == NR51)
        return mmu.mem[NR51] | 0x00; // useless?
    if (address == NR52)
        return mmu.mem[NR52] | 0x70;
    if (address > NR52 && address < WAVE_RAM)
        return 0xFF;

    return mmu.mem[address];
}

void mmu_write(word_t address, byte_t data) {
    if (address < VRAM) {
        write_mbc_registers(address, data);
    } else if (address >= ERAM && address < WRAM_BANK0) {
        write_mbc_eram(address, data);
    } else if (address == DIV) {
        // writing to DIV resets it to 0
        mmu.mem[address] = 0;
    } else if ((address >= OAM && address < UNUSABLE) && ((PPU_IS_MODE(PPU_OAM) || PPU_IS_MODE(PPU_DRAWING)) && CHECK_BIT(mmu.mem[LCDC], 7))) {
        // OAM inaccessible by cpu while ppu in mode 2 or 3 and LCD enabled
    } else if ((address >= VRAM && address < ERAM) && (PPU_IS_MODE(PPU_DRAWING) && CHECK_BIT(mmu.mem[LCDC], 7))) {
        // VRAM inaccessible by cpu while ppu in mode 3 and LCD enabled
    } else if (address >= UNUSABLE && address < IO) {
        // UNUSABLE memory is unusable
    } else if (address == LY) {
        // read only
    } else if (address == DMA) {
        // OAM DMA transfer
        // TODO this should not be instantaneous (it takes 640 cycles to complete and during that time the cpu can only access HRAM)
        memcpy(&mmu.mem[OAM], &mmu.mem[data * 0x100], 0xA0 * sizeof(byte_t));
        mmu.mem[address] = data;
    } else if (address == BANK && data == 1) {
        // disable boot rom
        memcpy(&mmu.mem, &mmu.cartridge, 0x100);
        mmu.mem[address] = data;
    } else if (address == P1) {
        // prevent writes to the lower nibble of the P1 register (joypad)
        mmu.mem[address] = data & 0xF0;
    } else if (address == NR10) {
        if (!IS_APU_ENABLED)
            return;
        mmu.mem[address] = data;
    } else if (address == NR11) {
        if (!IS_APU_ENABLED)
            return;
        mmu.mem[address] = data;
        apu.channel1.length_counter = 64 - (data & 0x3F);
    } else if (address == NR12) {
        if (!IS_APU_ENABLED)
            return;
        mmu.mem[address] = data;
        if (!(data >> 3)) // if dac disabled
            APU_DISABLE_CHANNEL(APU_CHANNEL_1);
    } else if (address == NR13) {
        if (!IS_APU_ENABLED)
            return;
        mmu.mem[address] = data;
    } else if (address == NR14) {
        if (!IS_APU_ENABLED)
            return;
        mmu.mem[address] = data;
        if (CHECK_BIT(data, 7) && (data >> 3)) // if trigger requested and dac enabled
            apu_channel_trigger(&apu.channel1);
    } else if (address == NR20) {
        if (!IS_APU_ENABLED)
            return;
        mmu.mem[address] = data;
    } else if (address == NR21) {
        if (!IS_APU_ENABLED)
            return;
        mmu.mem[address] = data;
        apu.channel2.length_counter = 64 - (data & 0x3F);
    } else if (address == NR22) {
        if (!IS_APU_ENABLED)
            return;
        mmu.mem[address] = data;
        if (!(data >> 3)) // if dac disabled
            APU_DISABLE_CHANNEL(APU_CHANNEL_2);
    } else if (address == NR23) {
        if (!IS_APU_ENABLED)
            return;
        mmu.mem[address] = data;
    } else if (address == NR24) {
        if (!IS_APU_ENABLED)
            return;
        mmu.mem[address] = data;
        if (CHECK_BIT(data, 7) && (data >> 3)) // if trigger requested and dac enabled
            apu_channel_trigger(&apu.channel2);
    } else if (address == NR30) {
        if (!IS_APU_ENABLED)
            return;
        mmu.mem[address] = data;
        if (!(data >> 7)) // if dac disabled
            APU_DISABLE_CHANNEL(APU_CHANNEL_3);
    } else if (address == NR31) {
        if (!IS_APU_ENABLED)
            return;
        mmu.mem[address] = data;
        apu.channel3.length_counter = 256 - data;
    } else if (address == NR32) {
        if (!IS_APU_ENABLED)
            return;
        mmu.mem[address] = data;
    } else if (address == NR33) {
        if (!IS_APU_ENABLED)
            return;
        mmu.mem[address] = data;
    } else if (address == NR34) {
        if (!IS_APU_ENABLED)
            return;
        mmu.mem[address] = data;
        if (CHECK_BIT(data, 7) && (data >> 7)) // if trigger requested and dac enabled
            apu_channel_trigger(&apu.channel3);
    } else if (address == NR40) {
        if (!IS_APU_ENABLED)
            return;
        mmu.mem[address] = data;
    } else if (address == NR41) {
        if (!IS_APU_ENABLED)
            return;
        mmu.mem[address] = data;
        apu.channel4.length_counter = 64 - (data & 0x3F);
    } else if (address == NR42) {
        if (!IS_APU_ENABLED)
            return;
        mmu.mem[address] = data;
        if (!(data >> 3)) // if dac disabled
            APU_DISABLE_CHANNEL(APU_CHANNEL_4);
    } else if (address == NR43) {
        if (!IS_APU_ENABLED)
            return;
        mmu.mem[address] = data;
    } else if (address == NR44) {
        if (!IS_APU_ENABLED)
            return;
        mmu.mem[address] = data;
        if (CHECK_BIT(data, 7) && (data >> 3)) // if trigger requested and dac enabled
            apu_channel_trigger(&apu.channel4);
    } else if (address == NR50) {
        if (!IS_APU_ENABLED)
            return;
        mmu.mem[address] = data;
    } else if (address == NR51) {
        if (!IS_APU_ENABLED)
            return;
        mmu.mem[address] = data;
    } else if (address == NR52) {
        CHANGE_BIT(mmu.mem[NR52], data >> 7, 7);
        if (!IS_APU_ENABLED)
            memset(&mmu.mem[NR10], 0x00, 32 * sizeof(byte_t)); // clear all registers
    } else if (address >= WAVE_RAM && address < LCDC) {
        if (!CHECK_BIT(mmu.mem[NR30], 7))
            mmu.mem[address] = data;
    } else if (address == STAT) {
        mmu.mem[address] = 0x80 | (data & 0x78) | (mmu.mem[address] & 0x07);
    } else {
        mmu.mem[address] = data;
    }
}
