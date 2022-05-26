#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "emulator.h"
#include "mmu.h"
#include "ppu.h"
#include "joypad.h"
#include "apu.h"
#include "boot.h"
#include "cpu.h"

static int parse_cartridge(emulator_t *emu) {
    mmu_t *mmu = emu->mmu;

    switch (mmu->cartridge[0x0147]) {
    case 0x00:
        mmu->mbc = MBC0;
        break;
    case 0x01: case 0x02: case 0x03:
        mmu->mbc = MBC1;
        break;
    case 0x05: case 0x06:
        mmu->mbc = MBC2;
        break;
    case 0x0F: case 0x11: case 0x12: case 0x13:
        mmu->mbc = MBC3;
        break;
    case 0x10:
        mmu->mbc = MBC30;
        break;
    case 0x19: case 0x1A: case 0x1B: case 0x1C: case 0x1D: case 0x1E:
        mmu->mbc = MBC5;
        break;
    // case 0x20:
    //     mmu->mbc = MBC6;
    //     break;
    // case 0x22:
    //     mmu->mbc = MBC7;
    //     break;
    default:
        eprintf("MBC byte %02X not supported\n", mmu->cartridge[0x0147]);
        return 0;
    }

    mmu->rom_banks = 2 << mmu->cartridge[0x0148];

    switch (mmu->cartridge[0x0149]) {
    case 0x00: mmu->eram_banks = 0; break;
    case 0x02: mmu->eram_banks = 1; break;
    case 0x03: mmu->eram_banks = 4; break;
    case 0x04: mmu->eram_banks = 16; break;
    case 0x05: mmu->eram_banks = 8; break;
    }

    // get rom title
    memcpy(emu->rom_title, (char *) &mmu->cartridge[0x134], 16);
    emu->rom_title[16] = '\0';
    if (mmu->cartridge[0x0143] == 0xC0 || mmu->cartridge[0x0143] == 0x80)
        emu->rom_title[15] = '\0';
    printf("Playing %s\n", emu->rom_title);
    printf("Cartridge using MBC%d with %d ROM banks + %d RAM banks\n", mmu->mbc == MBC30 ? 30 : mmu->mbc, mmu->rom_banks, mmu->eram_banks);

    // checksum validation
    int sum = 0;
    for (int i = 0x0134; i <= 0x014C; i++)
        sum = sum - mmu->cartridge[i] - 1;
    if (((byte_t) (sum & 0xFF)) != mmu->cartridge[0x014D]) {
        eprintf("invalid checksum\n");
        return 0;
    }

    // load cartridge into rom banks (everything before VRAM (0x8000))
    memcpy(mmu->mem, mmu->cartridge, VRAM);

    // Load bios after cartridge to overwrite first 0x100 bytes.
    memcpy(mmu->mem, dmg_boot, sizeof(dmg_boot));

    // load save into ERAM
    if (emu->save_filepath) {
        FILE *f = fopen(emu->save_filepath, "rb");
        // if there is a save file, load it into eram
        if (f) {
            fread(mmu->eram, 0x2000 * mmu->eram_banks, 1, f);
            // no fread check because a missing/invalid save file is not an error
            fclose(f);
        }
    }

    return 1;
}

int mmu_init(emulator_t *emu, char *rom_path, char *save_path) {
    const char *dot = strrchr(rom_path, '.');
    if (!dot || (strncmp(dot, ".gb", MAX(strlen(dot), sizeof(".gb"))) && strncmp(dot, ".gbc", MAX(strlen(dot), sizeof(".gbc"))))) {
        eprintf("%s: wrong file extension (expected .gb or .gbc)\n", rom_path);
        return 0;
    }

    mmu_t *mmu = xcalloc(1, sizeof(mmu_t));
    mmu->current_rom_bank = 1;

    if (save_path) {
        size_t len = strlen(save_path);
        emu->save_filepath = xmalloc(len + 2);
        snprintf(emu->save_filepath, len + 1, "%s", save_path);
    }

    size_t len = strlen(rom_path);
    emu->rom_filepath = xmalloc(len + 2);
    snprintf(emu->rom_filepath, len + 1, "%s", rom_path);

    FILE *f = fopen(rom_path, "rb");
    if (!f) {
        errnoprintf("opening file %s", rom_path);
        return 0;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = xmalloc(sizeof(mmu->cartridge));
    if (!fread(buf, fsize, 1, f)) {
        errnoprintf("reading %s", rom_path);
        fclose(f);
        return 0;
    }
    fclose(f);

    memcpy(mmu->cartridge, buf, fsize);
    free(buf);
    emu->mmu = mmu;
    return parse_cartridge(emu);
}

int mmu_init_from_data(emulator_t *emu, const byte_t *rom_data, size_t size, char *save_path) {
    mmu_t *mmu = xcalloc(1, sizeof(mmu_t));
    mmu->current_rom_bank = 1;

    if (save_path) {
        size_t len = strlen(save_path);
        emu->save_filepath = xmalloc(len + 2);
        snprintf(emu->save_filepath, len + 1, "%s", save_path);
    }

    memcpy(mmu->cartridge, rom_data, size);
    emu->mmu = mmu;
    return parse_cartridge(emu);
}

static int save_eram(emulator_t *emu) {
    if (!emu->mmu->eram_banks || !emu->save_filepath)
        return 0;

    make_parent_dirs(emu->rom_filepath);

    FILE *f = fopen(emu->save_filepath, "wb");
    if (!f) {
        errnoprintf("opening the save file");
        return 0;
    }
    if (!fwrite(emu->mmu->eram, 0x2000 * emu->mmu->eram_banks, 1, f)) {
        eprintf("writing to save file\n");
        fclose(f);
        return 0;
    }

    fclose(f);
    return 1;
}

void mmu_quit(emulator_t *emu) {
    save_eram(emu);

    if (emu->rom_filepath)
        free(emu->rom_filepath);
    if (emu->save_filepath)
        free(emu->save_filepath);
    free(emu->mmu);
}

static void rtc_update(mmu_t *mmu) {
    // get current time in seconds
    time_t current_update_time = time(NULL);

    // get time elapsed
    time_t elapsed = current_update_time - mmu->rtc.value_in_seconds;

    mmu->rtc.value_in_seconds += elapsed;

    word_t d = mmu->rtc.value_in_seconds / 86400;
    elapsed %= 86400;
    if (d >= 0x0200) { // day overflow
        SET_BIT(mmu->rtc.dh, 7);
        d %= 0x0200;
    }
    mmu->rtc.dh |= (d & 0x100) >> 8;
    mmu->rtc.dl = d & 0xFF;

    mmu->rtc.h = elapsed / 3600;
    elapsed %= 3600;

    mmu->rtc.m = elapsed / 60;
    elapsed %= 60;

    mmu->rtc.s = elapsed;
}

// TODO MBC1 special cases where rom bank 0 is changed are not implemented
static void write_mbc_registers(mmu_t *mmu, word_t address, byte_t data) {
    switch (mmu->mbc) {
    case MBC0:
        break; // read only, do nothing
    case MBC1:
        if (address < 0x2000) {
            mmu->eram_enabled = (data & 0x0F) == 0x0A;
        } else if (address < 0x4000) {
            mmu->current_rom_bank = data & 0x1F;
            if (mmu->current_rom_bank == 0x00)
                mmu->current_rom_bank = 0x01;
            mmu->current_rom_bank &= mmu->rom_banks - 1; // in this case, equivalent to current_rom_bank %= rom_banks but avoid division by 0
        } else if (address < 0x6000) {
            if (mmu->mbc1_mode) { // ROM mode
                mmu->current_eram_bank = data & 0x03;
                mmu->current_eram_bank &= mmu->eram_banks - 1; // in this case, equivalent to current_eram_bank %= eram_banks but avoid division by 0
            } else {
                mmu->current_rom_bank = ((data & 0x03) << 5) | (mmu->current_rom_bank & 0x1F);
                // commenting this passes more tests but I think its necessary to keep it so the problem may come from the read instead of the write?
                // if (mmu->current_rom_bank == 0x00 || mmu->current_rom_bank == 0x20 || mmu->current_rom_bank == 0x40 || mmu->current_rom_bank == 0x60)
                //     mmu->current_rom_bank++;
                mmu->current_rom_bank &= mmu->rom_banks - 1; // in this case, equivalent to current_rom_bank %= rom_banks but avoid division by 0
                mmu->current_eram_bank = 0x00;
            }
        } else if (address < 0x8000) {
            mmu->mbc1_mode = data & 0x01;
        }
        break;
    case MBC2:
        if (address >= 0x4000)
            break;

        if (!CHECK_BIT(address, 8)) {
            mmu->eram_enabled = (data & 0x0F) == 0x0A;
        } else {
            mmu->current_rom_bank = data & 0x0F;
            if (mmu->current_rom_bank == 0x00)
                mmu->current_rom_bank = 0x01; // 0x00 not allowed
            mmu->current_rom_bank &= mmu->rom_banks - 1; // in this case, equivalent to current_rom_bank %= rom_banks but avoid division by 0
        }
        break;
    case MBC3:
    case MBC30:
        if (address < 0x2000) {
            mmu->eram_enabled = (data & 0x0F) == 0x0A;
            mmu->rtc.enabled = (data & 0x0F) == 0x0A;
        } else if (address < 0x4000) {
            mmu->current_rom_bank = mmu->mbc == MBC30 ? data : data & 0x7F;
            if (mmu->current_rom_bank == 0x00)
                mmu->current_rom_bank = 0x01; // 0x00 not allowed
            mmu->current_rom_bank &= mmu->rom_banks - 1; // in this case, equivalent to current_rom_bank %= rom_banks but avoid division by 0
        } else if (address < 0x6000) {
            if (data <= 0x03) {
                mmu->current_eram_bank = data;
                mmu->current_eram_bank &= mmu->eram_banks - 1; // in this case, equivalent to current_eram_bank %= eram_banks but avoid division by 0
            } else if (data >= 0x08 && data <= 0x0C) {
                mmu->rtc.reg = data;
                mmu->current_eram_bank = -1;
            }
        } else if (address < 0x8000) {
            if (mmu->rtc.latch == 0x00 && data == 0x01 && !CHECK_BIT(mmu->rtc.dh, 6))
                rtc_update(mmu);
            mmu->rtc.latch = data;
        }
        break;
    case MBC5:
        if (address < 0x2000) {
            mmu->eram_enabled = (data & 0x0F) == 0x0A;
        } else if (address < 0x3000) {
            mmu->current_rom_bank = (mmu->current_rom_bank & 0x100) | data;
            mmu->current_rom_bank &= mmu->rom_banks - 1; // in this case, equivalent to current_rom_bank %= rom_banks but avoid division by 0
        } else if (address < 0x4000) {
            mmu->current_rom_bank = ((data & 1) << 8) | mmu->current_rom_bank;
            mmu->current_rom_bank &= mmu->rom_banks - 1; // in this case, equivalent to current_rom_bank %= rom_banks but avoid division by 0
        } else if (address < 0x6000) {
            mmu->current_eram_bank = data & 0x0F;
            mmu->current_eram_bank &= mmu->eram_banks - 1; // in this case, equivalent to current_eram_bank %= eram_banks but avoid division by 0
            // TODO On cartridges which feature a rumble motor, bit 3 controls the rumble motor instead of the RAM chip.
        }
        break;
    default:
        break;
    }
}

static void write_mbc_eram(mmu_t *mmu, word_t address, byte_t data) {
    switch (mmu->mbc) {
    case MBC1:
        if (mmu->eram_enabled)
            mmu->eram[(address - 0xA000) + (mmu->current_eram_bank * 0x2000)] = data;
        break;
    case MBC2:
        if (mmu->eram_enabled)
            mmu->eram[(address - 0xA000) + (mmu->current_eram_bank * 0x2000)] = data & 0x0F;
        break;
    case MBC3:
    case MBC30:
        if (mmu->eram_enabled && mmu->current_eram_bank >= 0) {
            mmu->eram[(address - 0xA000) + (mmu->current_eram_bank * 0x2000)] = data;
        } else if (mmu->rtc.enabled) {
            switch (mmu->rtc.reg) {
            case 0x08: mmu->rtc.s = data; break;
            case 0x09: mmu->rtc.m = data; break;
            case 0x0A: mmu->rtc.h = data; break;
            case 0x0B: mmu->rtc.dl = data; break;
            case 0x0C: mmu->rtc.dh = data; break;
            default: break;
            }
        }
        break;
    case MBC5:
        if (mmu->eram_enabled)
            mmu->eram[(address - 0xA000) + (mmu->current_eram_bank * 0x2000)] = data;
        // TODO MBC5 eram is the same as for MBC1, except that RAM sizes are 8 KiB, 32 KiB and 128 KiB.
        break;
    default:
        break;
    }
}

byte_t mmu_read(emulator_t *emu, word_t address) {
    mmu_t *mmu = emu->mmu;

    if (mmu->mbc >= MBC1 && mmu->mbc <= MBC5) {
        if (address >= 0x4000 && address < 0x8000) // ROM_BANKN
            return mmu->cartridge[(address - 0x4000) + (mmu->current_rom_bank * 0x4000)];

        if (address >= 0xA000 && address < 0xC000) { // ERAM
            if (mmu->current_eram_bank >= 0) {
                return mmu->eram_enabled ? mmu->eram[(address - 0xA000) + (mmu->current_eram_bank * 0x2000)] : 0xFF;
            } else if (mmu->rtc.enabled) { // implies mbc == MBC3 because rtc_enabled is set to 0 by default
                switch (mmu->rtc.reg) {
                case 0x08: return mmu->rtc.s;
                case 0x09: return mmu->rtc.m;
                case 0x0A: return mmu->rtc.h;
                case 0x0B: return mmu->rtc.dl;
                case 0x0C: return mmu->rtc.dh;
                default: return 0xFF;
                }
            }
        }
    }

    // OAM inaccessible by cpu while ppu in mode 2 or 3 and LCD is enabled (return undefined data)
    if (address >= OAM && address < UNUSABLE && CHECK_BIT(mmu->mem[LCDC], 7) && ((PPU_IS_MODE(emu, PPU_MODE_OAM) || PPU_IS_MODE(emu, PPU_MODE_DRAWING))))
        return 0xFF;

    // VRAM inaccessible by cpu while ppu in mode 3 and LCD is enabled (return undefined data)
    if ((address >= VRAM && address < ERAM) && CHECK_BIT(mmu->mem[LCDC], 7) && PPU_IS_MODE(emu, PPU_MODE_DRAWING))
        return 0xFF;

    // UNUSABLE memory is unusable
    if (address >= UNUSABLE && address < IO)
        return 0xFF;

    // Reading from P1 register returns joypad input state according to its current bit 4 or 5 value
    if (address == P1)
        return joypad_get_input(emu);

    if (address == NR10)
        return mmu->mem[NR10] | 0x80;
    if (address == NR11)
        return mmu->mem[NR11] | 0x3F;
    if (address == NR12)
        return mmu->mem[NR12] | 0x00; // useless?
    if (address == NR13)
        return 0xFF;
    if (address == NR14)
        return mmu->mem[NR14] | 0xBF;

    if (address == NR20)
        return 0xFF;
    if (address == NR21)
        return mmu->mem[NR21] | 0x3F;
    if (address == NR22)
        return mmu->mem[NR22] | 0x00; // useless?
    if (address == NR23)
        return 0xFF;
    if (address == NR24)
        return mmu->mem[NR24] | 0xBF;

    if (address == NR30)
        return mmu->mem[NR30] | 0x7F;
    if (address == NR31)
        return 0xFF;
    if (address == NR32)
        return mmu->mem[NR32] | 0x9F;
    if (address == NR33)
        return 0xFF;
    if (address == NR34)
        return mmu->mem[NR34] | 0xBF;

    if (address == NR40)
        return 0xFF;
    if (address == NR41)
        return 0xFF;
    if (address == NR42)
        return mmu->mem[NR42] | 0x00; // useless?
    if (address == NR43)
        return mmu->mem[NR43] | 0x00; // useless?
    if (address == NR44)
        return mmu->mem[NR44] | 0xBF;

    if (address == NR50)
        return mmu->mem[NR50] | 0x00; // useless?
    if (address == NR51)
        return mmu->mem[NR51] | 0x00; // useless?
    if (address == NR52)
        return mmu->mem[NR52] | 0x70;
    if (address > NR52 && address < WAVE_RAM)
        return 0xFF;

    return mmu->mem[address];
}

void mmu_write(emulator_t* emu, word_t address, byte_t data) {
    mmu_t *mmu = emu->mmu;

    if (address < VRAM) {
        write_mbc_registers(mmu, address, data);
    } else if (address >= ERAM && address < WRAM_BANK0) {
        write_mbc_eram(mmu, address, data);
    } else if (address == DIV) {
        // writing to DIV resets it to 0
        mmu->mem[address] = 0;
    } else if ((address >= OAM && address < UNUSABLE) && ((PPU_IS_MODE(emu, PPU_MODE_OAM) || PPU_IS_MODE(emu, PPU_MODE_DRAWING)) && CHECK_BIT(mmu->mem[LCDC], 7))) {
        // OAM inaccessible by cpu while ppu in mode 2 or 3 and LCD enabled
    } else if ((address >= VRAM && address < ERAM) && (PPU_IS_MODE(emu, PPU_MODE_DRAWING) && CHECK_BIT(mmu->mem[LCDC], 7))) {
        // VRAM inaccessible by cpu while ppu in mode 3 and LCD enabled
    } else if (address >= UNUSABLE && address < IO) {
        // UNUSABLE memory is unusable
    } else if (address == LY) {
        // read only
    } else if (address == LYC) {
        // a write to LYC triggers an immediate LY=LYC comparison
        mmu->mem[address] = data;
        ppu_ly_lyc_compare(emu);
    } else if (address == DMA) {
        // OAM DMA transfer
        // TODO this should not be instantaneous (it takes 640 cycles to complete and during that time the cpu can only access HRAM)
        memcpy(&mmu->mem[OAM], &mmu->mem[data * 0x100], 0xA0 * sizeof(byte_t));
        mmu->mem[address] = data;
    } else if (address == BANK && data == 1) {
        // disable boot rom
        memcpy(mmu->mem, mmu->cartridge, 0x100);
        mmu->mem[address] = data;
    } else if (address == P1) {
        // prevent writes to the lower nibble of the P1 register (joypad)
        mmu->mem[address] = data & 0xF0;
    } else if (address == NR10) {
        if (!IS_APU_ENABLED(emu))
            return;
        mmu->mem[address] = data;
    } else if (address == NR11) {
        if (!IS_APU_ENABLED(emu))
            return;
        mmu->mem[address] = data;
        emu->apu->channel1.length_counter = 64 - (data & 0x3F);
    } else if (address == NR12) {
        if (!IS_APU_ENABLED(emu))
            return;
        mmu->mem[address] = data;
        if (!(data >> 3)) // if dac disabled
            APU_DISABLE_CHANNEL(emu, APU_CHANNEL_1);
    } else if (address == NR13) {
        if (!IS_APU_ENABLED(emu))
            return;
        mmu->mem[address] = data;
    } else if (address == NR14) {
        if (!IS_APU_ENABLED(emu))
            return;
        mmu->mem[address] = data;
        if (CHECK_BIT(data, 7) && (data >> 3)) // if trigger requested and dac enabled
            apu_channel_trigger(emu, &emu->apu->channel1);
    } else if (address == NR20) {
        if (!IS_APU_ENABLED(emu))
            return;
        mmu->mem[address] = data;
    } else if (address == NR21) {
        if (!IS_APU_ENABLED(emu))
            return;
        mmu->mem[address] = data;
        emu->apu->channel2.length_counter = 64 - (data & 0x3F);
    } else if (address == NR22) {
        if (!IS_APU_ENABLED(emu))
            return;
        mmu->mem[address] = data;
        if (!(data >> 3)) // if dac disabled
            APU_DISABLE_CHANNEL(emu, APU_CHANNEL_2);
    } else if (address == NR23) {
        if (!IS_APU_ENABLED(emu))
            return;
        mmu->mem[address] = data;
    } else if (address == NR24) {
        if (!IS_APU_ENABLED(emu))
            return;
        mmu->mem[address] = data;
        if (CHECK_BIT(data, 7) && (data >> 3)) // if trigger requested and dac enabled
            apu_channel_trigger(emu, &emu->apu->channel2);
    } else if (address == NR30) {
        if (!IS_APU_ENABLED(emu))
            return;
        mmu->mem[address] = data;
        if (!(data >> 7)) // if dac disabled
            APU_DISABLE_CHANNEL(emu, APU_CHANNEL_3);
    } else if (address == NR31) {
        if (!IS_APU_ENABLED(emu))
            return;
        mmu->mem[address] = data;
        emu->apu->channel3.length_counter = 256 - data;
    } else if (address == NR32) {
        if (!IS_APU_ENABLED(emu))
            return;
        mmu->mem[address] = data;
    } else if (address == NR33) {
        if (!IS_APU_ENABLED(emu))
            return;
        mmu->mem[address] = data;
    } else if (address == NR34) {
        if (!IS_APU_ENABLED(emu))
            return;
        mmu->mem[address] = data;
        if (CHECK_BIT(data, 7) && (data >> 7)) // if trigger requested and dac enabled
            apu_channel_trigger(emu, &emu->apu->channel3);
    } else if (address == NR40) {
        if (!IS_APU_ENABLED(emu))
            return;
        mmu->mem[address] = data;
    } else if (address == NR41) {
        if (!IS_APU_ENABLED(emu))
            return;
        mmu->mem[address] = data;
        emu->apu->channel4.length_counter = 64 - (data & 0x3F);
    } else if (address == NR42) {
        if (!IS_APU_ENABLED(emu))
            return;
        mmu->mem[address] = data;
        if (!(data >> 3)) // if dac disabled
            APU_DISABLE_CHANNEL(emu, APU_CHANNEL_4);
    } else if (address == NR43) {
        if (!IS_APU_ENABLED(emu))
            return;
        mmu->mem[address] = data;
    } else if (address == NR44) {
        if (!IS_APU_ENABLED(emu))
            return;
        mmu->mem[address] = data;
        if (CHECK_BIT(data, 7) && (data >> 3)) // if trigger requested and dac enabled
            apu_channel_trigger(emu, &emu->apu->channel4);
    } else if (address == NR50) {
        if (!IS_APU_ENABLED(emu))
            return;
        mmu->mem[address] = data;
    } else if (address == NR51) {
        if (!IS_APU_ENABLED(emu))
            return;
        mmu->mem[address] = data;
    } else if (address == NR52) {
        CHANGE_BIT(mmu->mem[NR52], data >> 7, 7);
        if (!IS_APU_ENABLED(emu))
            memset(&mmu->mem[NR10], 0x00, 32 * sizeof(byte_t)); // clear all registers
    } else if (address >= WAVE_RAM && address < LCDC) {
        if (!CHECK_BIT(mmu->mem[NR30], 7))
            mmu->mem[address] = data;
    } else if (address == STAT) {
        mmu->mem[address] = 0x80 | (data & 0x78) | (mmu->mem[address] & 0x07);
    } else {
        mmu->mem[address] = data;
    }
}
