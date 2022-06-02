#include <stdlib.h>
#include <stddef.h>
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

const char *mbc_names[] = {
    "ROM_ONLY",
    "MBC1",
    "MBC2",
    "MBC3",
    "MBC30",
    "MBC5",
    "MBC6",
    "MBC7",
    "HuC1"
};

static inline void rtc_update(rtc_t *rtc) {
    time_t now = time(NULL);
    time_t elapsed = now - rtc->timestamp;
    rtc->timestamp = now;
    rtc->value_in_seconds += elapsed;
    time_t value_in_seconds = rtc->value_in_seconds;

    word_t d = value_in_seconds / 86400;
    value_in_seconds %= 86400;

    // day overflow
    if (d >= 0x0200) {
        SET_BIT(rtc->dh, 7);
        d %= 0x0200;
    }

    rtc->dh |= (d & 0x100) >> 8;
    rtc->dl = d & 0xFF;

    rtc->h = value_in_seconds / 3600;
    value_in_seconds %= 3600;

    rtc->m = value_in_seconds / 60;
    value_in_seconds %= 60;

    rtc->s = value_in_seconds;

    // if there was a day overflow, emulate the overflow on rtc->value_in_seconds
    if (CHECK_BIT(rtc->dh, 7))
        rtc->value_in_seconds = rtc->s + rtc->m * 60 + rtc->h * 3600 + d * 86400;
}

static int parse_cartridge(emulator_t *emu) {
    mmu_t *mmu = emu->mmu;

    switch (mmu->cartridge[0x0147]) {
    case 0x00:
        mmu->mbc = ROM_ONLY;
        break;
    case 0x01:
        mmu->mbc = MBC1;
        break;
    case 0x02:
        mmu->mbc = MBC1;
        mmu->has_eram = 1;
        break;
    case 0x03:
        mmu->mbc = MBC1;
        mmu->has_eram = 1;
        mmu->has_battery = 1;
        break;
    case 0x05:
        mmu->mbc = MBC2;
        break;
    case 0x06:
        mmu->mbc = MBC2;
        mmu->has_battery = 1;
        break;
    case 0x0F:
        mmu->mbc = MBC3;
        mmu->has_rtc = 1;
        mmu->has_battery = 1;
    case 0x10:
        mmu->mbc = MBC3;
        mmu->has_rtc = 1;
        mmu->has_battery = 1;
        mmu->has_eram = 1;
        break;
    case 0x11:
        mmu->mbc = MBC3;
        break;
    case 0x12:
        mmu->mbc = MBC3;
        mmu->has_eram = 1;
        break;
    case 0x13:
        mmu->mbc = MBC3;
        mmu->has_battery = 1;
        mmu->has_eram = 1;
        break;
    case 0x19:
        mmu->mbc = MBC5;
        break;
    case 0x1A:
        mmu->mbc = MBC5;
        mmu->has_eram = 1;
        break;
    case 0x1B:
        mmu->mbc = MBC5;
        mmu->has_eram = 1;
        mmu->has_battery = 1;
        break;
    case 0x1C:
        mmu->mbc = MBC5;
        mmu->has_rumble = 1;
        break;
    case 0x1D:
        mmu->mbc = MBC5;
        mmu->has_rumble = 1;
        mmu->has_eram = 1;
        break;
    case 0x1E:
        mmu->mbc = MBC5;
        mmu->has_rumble = 1;
        mmu->has_eram = 1;
        mmu->has_battery = 1;
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

    if (mmu->mbc == MBC3 && mmu->eram_banks == 8)
        mmu->mbc = MBC30;

    // get rom title
    memcpy(emu->rom_title, (char *) &mmu->cartridge[0x134], 16);
    emu->rom_title[16] = '\0';
    if (mmu->cartridge[0x0143] == 0xC0 || mmu->cartridge[0x0143] == 0x80)
        emu->rom_title[15] = '\0';

    printf("[%s] Playing %s\n", emu->mode == DMG ? "DMG" : "CGB", emu->rom_title);

    char *ram_str = NULL;
    if (mmu->has_eram) {
        ram_str = xmalloc(18);
        snprintf(ram_str, 17, " + %d RAM banks", mmu->eram_banks);
    }

    printf("Cartridge using %s with %d ROM banks%s%s%s%s\n",
            mbc_names[mmu->mbc],
            mmu->rom_banks,
            ram_str ? ram_str : "",
            mmu->has_battery ? " + BATTERY" : "",
            mmu->has_rtc ? " + RTC" : "",
            mmu->has_rumble ? " + RUMBLE" : ""
    );
    free(ram_str);

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

    if (emu->mode == DMG) {
        // Load bios after cartridge to overwrite first 0x100 bytes.
        memcpy(mmu->mem, dmg_boot, sizeof(dmg_boot));
    } else {
        // Load first part of the bios after cartridge to overwrite first 0x100 bytes.
        memcpy(mmu->mem, cgb_boot, 0x100);
        // Load second part of the bios.
        memcpy(&mmu->mem[0x200], &cgb_boot[0x200], sizeof(cgb_boot) - 0x200);
    }

    // load save into ERAM
    if (emu->mmu->has_battery && emu->save_filepath) {
        FILE *f = fopen(emu->save_filepath, "rb");
        // if there is a save file, load it into eram
        if (f) {
            // no fread checks because a missing/invalid save file is not an error

            if (mmu->eram_banks > 0)
                fread(mmu->eram, 0x2000 * mmu->eram_banks, 1, f);

            if (mmu->has_rtc) {
                fread(&mmu->rtc.value_in_seconds, sizeof(rtc_t) - offsetof(rtc_t, value_in_seconds), 1, f);
                rtc_update(&mmu->rtc);
            }

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
    mmu->cartridge_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = xmalloc(mmu->cartridge_size);
    if (!fread(buf, mmu->cartridge_size, 1, f)) {
        errnoprintf("reading %s", rom_path);
        fclose(f);
        return 0;
    }
    fclose(f);

    memcpy(mmu->cartridge, buf, mmu->cartridge_size);
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

    mmu->cartridge_size = size;
    memcpy(mmu->cartridge, rom_data, mmu->cartridge_size);
    emu->mmu = mmu;
    return parse_cartridge(emu);
}

static int save(emulator_t *emu) {
    // don't save if the cartridge has no battery or no save path has been given or their is no rtc and no eram banks
    if (!emu->mmu->has_battery || !emu->save_filepath || (!emu->mmu->has_rtc && emu->mmu->eram_banks == 0))
        return 0;

    make_parent_dirs(emu->rom_filepath);

    FILE *f = fopen(emu->save_filepath, "wb");
    if (!f) {
        errnoprintf("opening the save file");
        return 0;
    }

    if (emu->mmu->eram_banks > 0) {
        if (!fwrite(emu->mmu->eram, 0x2000 * emu->mmu->eram_banks, 1, f)) {
            eprintf("writing eram to save file\n");
            fclose(f);
            return 0;
        }
    }

    if (emu->mmu->has_rtc) {
        if (!fwrite(&emu->mmu->rtc.value_in_seconds, sizeof(rtc_t) - offsetof(rtc_t, value_in_seconds), 1, f)) {
            eprintf("writing rtc to save file\n");
            fclose(f);
            return 0;
        }
    }

    fclose(f);
    return 1;
}

void mmu_quit(emulator_t *emu) {
    save(emu);

    if (emu->rom_filepath)
        free(emu->rom_filepath);
    if (emu->save_filepath)
        free(emu->save_filepath);
    free(emu->mmu);
}

void mmu_step(emulator_t *emu, int cycles) {
    while (cycles-- > 0) {
        // step rtc clock if it's present and is not halted
        if (emu->mmu->has_rtc && !CHECK_BIT(emu->mmu->rtc.dh, 6)) {
            emu->mmu->rtc.cpu_cycles_counter++;
            if (emu->mmu->rtc.cpu_cycles_counter >= GB_CPU_FREQ / 32768) { // 32768 Hz
                emu->mmu->rtc.cpu_cycles_counter = 0;

                emu->mmu->rtc.rtc_cycles_counter++;
                if (CHECK_BIT(emu->mmu->rtc.rtc_cycles_counter, 15)) {
                    emu->mmu->rtc.rtc_cycles_counter = 0;
                    rtc_update(&emu->mmu->rtc);
                }
            }
        }

        // TODO update OAM DMA
        // mmu->oam_dma_cycles_counter++;

        if (emu->mode == CGB) {
            // TODO update HDMA

            // TODO update GDMA
        }
    }
}

// TODO MBC1 special cases where rom bank 0 is changed are not implemented
static void write_mbc_registers(mmu_t *mmu, word_t address, byte_t data) {
    switch (mmu->mbc) {
    case ROM_ONLY:
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
                // TODO commenting this passes more tests but I think its necessary to keep it so the problem may come from the read instead of the write?
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
            if (mmu->has_rtc)
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
            } else if (mmu->has_rtc && data >= 0x08 && data <= 0x0C) {
                mmu->rtc.reg = data;
                mmu->current_eram_bank = -1;
            }
        } else if (address < 0x8000 && mmu->has_rtc) {
            if (mmu->rtc.latch == 0x00 && data == 0x01 && !CHECK_BIT(mmu->rtc.dh, 6)) {
                mmu->rtc.latched_s = mmu->rtc.s;
                mmu->rtc.latched_m = mmu->rtc.m;
                mmu->rtc.latched_h = mmu->rtc.h;
                mmu->rtc.latched_dl = mmu->rtc.dl;
                mmu->rtc.latched_dh = mmu->rtc.dh;
            }
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
        if (mmu->has_eram && mmu->eram_enabled)
            mmu->eram[(address - 0xA000) + (mmu->current_eram_bank * 0x2000)] = data;
        break;
    case MBC2:
        if (mmu->has_eram && mmu->eram_enabled)
            mmu->eram[(address - 0xA000) + (mmu->current_eram_bank * 0x2000)] = data & 0x0F;
        break;
    case MBC3:
    case MBC30:
        if (mmu->has_eram && mmu->eram_enabled && mmu->current_eram_bank >= 0) {
            mmu->eram[(address - 0xA000) + (mmu->current_eram_bank * 0x2000)] = data;
        } else if (mmu->has_rtc && mmu->rtc.enabled) {
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
        if (mmu->has_eram && mmu->eram_enabled)
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
        if (address >= ROM_BANKN && address < VRAM) // ROM_BANKN
            return mmu->cartridge[(address - ROM_BANKN) + (mmu->current_rom_bank * 0x4000)];

        if (address >= 0xA000 && address < 0xC000) { // ERAM
            if (mmu->has_eram && mmu->current_eram_bank >= 0) {
                return mmu->eram_enabled ? mmu->eram[(address - 0xA000) + (mmu->current_eram_bank * 0x2000)] : 0xFF;
            } else if (mmu->has_rtc && mmu->rtc.enabled) { // implies mbc == MBC3 (or MBC30) because rtc_enabled is set to 0 by default
                switch (mmu->rtc.reg) {
                case 0x08: return mmu->rtc.latched_s;
                case 0x09: return mmu->rtc.latched_m;
                case 0x0A: return mmu->rtc.latched_h;
                case 0x0B: return mmu->rtc.latched_dl;
                case 0x0C: return mmu->rtc.latched_dh;
                default: return 0xFF;
                }
            }
        }
    }

    // OAM inaccessible by cpu while ppu in mode 2 or 3 and LCD is enabled (return undefined data)
    if (address >= OAM && address < UNUSABLE && CHECK_BIT(mmu->mem[LCDC], 7) && ((PPU_IS_MODE(emu, PPU_MODE_OAM) || PPU_IS_MODE(emu, PPU_MODE_DRAWING))))
        return 0xFF;

    if (address >= VRAM && address < ERAM) {
        // VRAM inaccessible by cpu while ppu in mode 3 and LCD is enabled (return undefined data)
        if (CHECK_BIT(mmu->mem[LCDC], 7) && PPU_IS_MODE(emu, PPU_MODE_DRAWING))
            return 0xFF;

        if (emu->mode == CGB && CHECK_BIT(mmu->mem[VBK], 0))
            return mmu->vram_extra[(address - VRAM)];
        else
            return mmu->mem[address];
    }

    // If in CGB mode, access to extra wram banks
    if (emu->mode == CGB && address >= WRAM_BANKN && address < ECHO) {
        byte_t current_wram_bank = mmu->mem[SVBK] & 0x07;
        if (current_wram_bank == 0)
            current_wram_bank = 1;
        return mmu->wram_extra[(address - WRAM_BANKN) + ((current_wram_bank - 1) * 0x1000)];
    }

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

    if (address == 0xFF72)
        return emu->mode == CGB ? mmu->mem[address] : 0xFF; // only readable in CGB mode

    if (emu->mode == CGB) {
        if (address == 0xFF75)
            return mmu->mem[address] & 0x70;

        // TODO The low nibble is a copy of sound channel #1’s PCM amplitude, the high nibble a copy of sound channel #2’s.
        // if (address == PCM12)
        //     return 0xFF;

        // TODO The low nibble is a copy of sound channel #3’s PCM amplitude, the high nibble a copy of sound channel #4’s.
        // if (address == PCM34)
        //     return 0xFF;

        if (address == VBK)
            return 0xFE | (mmu->mem[address] & 0x01);

        if (address == SVBK)
            return 0xF8 | (mmu->mem[address] & 0x07);

        if (address == BGPD) {
            if (PPU_IS_MODE(emu, PPU_MODE_DRAWING)) {
                return 0xFF;
            } else {
                byte_t cram_address = mmu->mem[BGPI] & 0x3F;
                return mmu->cram_bg[cram_address];
            }
        }

        if (address == OBPD) {
            if (PPU_IS_MODE(emu, PPU_MODE_DRAWING)) {
                return 0xFF;
            } else {
                byte_t cram_address = mmu->mem[OBPI] & 0x3F;
                return mmu->cram_obj[cram_address];
            }
        }
    }

    return mmu->mem[address];
}

void mmu_write(emulator_t* emu, word_t address, byte_t data) {
    mmu_t *mmu = emu->mmu;

    if (address < VRAM) {
        write_mbc_registers(mmu, address, data);
    } else if (address >= VRAM && address < ERAM) {
        // VRAM inaccessible by cpu while ppu in mode 3 and LCD is enabled (return undefined data)
        if (CHECK_BIT(mmu->mem[LCDC], 7) && PPU_IS_MODE(emu, PPU_MODE_DRAWING))
            return;

        if (emu->mode == CGB && CHECK_BIT(mmu->mem[VBK], 0))
            mmu->vram_extra[(address - VRAM)] = data;
        else
            mmu->mem[address] = data;
    } else if (address >= ERAM && address < WRAM_BANK0) {
        write_mbc_eram(mmu, address, data);
    } else if (emu->mode == CGB && address >= WRAM_BANKN && address < ECHO) {
        byte_t current_wram_bank = mmu->mem[SVBK] & 0x07;
        if (current_wram_bank == 0)
            current_wram_bank = 1;
        mmu->wram_extra[(address - WRAM_BANKN) + ((current_wram_bank - 1) * 0x1000)] = data;
    } else if ((address >= OAM && address < UNUSABLE) && ((PPU_IS_MODE(emu, PPU_MODE_OAM) || PPU_IS_MODE(emu, PPU_MODE_DRAWING)) && CHECK_BIT(mmu->mem[LCDC], 7))) {
        // OAM inaccessible by cpu while ppu in mode 2 or 3 and LCD enabled
    } else if (address >= UNUSABLE && address < IO) {
        // UNUSABLE memory is unusable
    } else if (address == DIV) {
        // writing to DIV resets it to 0
        mmu->mem[address] = 0;
    } else if (address == LY) {
        // read only
    } else if (address == LYC) {
        // a write to LYC triggers an immediate LY=LYC comparison
        mmu->mem[address] = data;
        ppu_ly_lyc_compare(emu);
    } else if (address == DMA) {
        // OAM DMA transfer
        // TODO this should not be instantaneous (it takes 640 cycles to complete and during that time the cpu can only access HRAM)
        memcpy(&mmu->mem[OAM], &mmu->mem[data * 0x100], 0xA0);
        mmu->mem[address] = data;
    } else if (address == BANK) {
        // disable boot rom
        if (emu->mode == DMG && data == 0x01) {
            memcpy(mmu->mem, mmu->cartridge, 0x100);
        } else if (emu->mode == CGB && data == 0x11) {
            memcpy(mmu->mem, mmu->cartridge, 0x100);
            memcpy(&mmu->mem[0x200], &mmu->cartridge[0x200], sizeof(cgb_boot) - 0x200);
        }
        mmu->mem[address] = data;
    } else if (address == HDMA5 && emu->mode == CGB) {
        // writing to this register starts a VRAM DMA transfer
        word_t src_address = ((mmu->mem[HDMA1] << 8) | mmu->mem[HDMA2]) & 0xFFF0;
        word_t dest_address = ((mmu->mem[HDMA3] << 8) | mmu->mem[HDMA4]) & 0x1FF0;
        word_t len = ((data & 0x7F) + 1) * 0x10;
        // TODO both these DMA should not be instantaneous and HBLANK DMA can even be cancelled
        //      instantaneous HDMA/GDMA will cause visual glitches
        if (CHECK_BIT(data, 7)) { // HBLANK DMA (HDMA)
            if (CHECK_BIT(mmu->mem[VBK], 0))
                memcpy(&mmu->vram_extra[dest_address], &mmu->mem[src_address], len);
            else
                memcpy(&mmu->mem[VRAM + dest_address], &mmu->mem[src_address], len);
            mmu->mem[address] = 0xFF;
            printf("HDMA length=%d, vram bank=%d, src=%x, dest=%x\n", len, mmu->mem[VBK] & 0x01, src_address, dest_address + VRAM);
        } else { // General purpose DMA (GDMA)
            if (CHECK_BIT(mmu->mem[VBK], 0))
                memcpy(&mmu->vram_extra[dest_address], &mmu->mem[src_address], len);
            else
                memcpy(&mmu->mem[VRAM + dest_address], &mmu->mem[src_address], len);
            mmu->mem[address] = 0xFF;
            printf("GDMA length=%d, vram bank=%d, src=%x, dest=%x\n", len, mmu->mem[VBK] & 0x01, src_address, dest_address + VRAM);
        }
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
    } else if (address == BGPD && emu->mode == CGB) {
        if (!PPU_IS_MODE(emu, PPU_MODE_DRAWING)) {
            byte_t cram_address = mmu->mem[BGPI] & 0x3F;
            mmu->cram_bg[cram_address] = data;
            // printf("write %d in cram_bg %d\n", data, cram_address);
        }

        // increment BGPI address if auto increment (bit.7) of BGPI is set
        if (CHECK_BIT(mmu->mem[BGPI], 7)) {
            byte_t new_bgpi_address = (mmu->mem[BGPI] & 0x3F) + 1;
            if (new_bgpi_address > 0x3F)
                new_bgpi_address = 0;
            mmu->mem[BGPI] = new_bgpi_address;
            SET_BIT(mmu->mem[BGPI], 7);
        }
    } else if (address == OBPD && emu->mode == CGB) {
        if (!PPU_IS_MODE(emu, PPU_MODE_DRAWING)) {
            byte_t cram_address = mmu->mem[OBPI] & 0x3F;
            mmu->cram_obj[cram_address] = data;
            // printf("write %d in cram_obj %d\n", data, cram_address);
        }

        // increment OBPI address if auto increment (bit.7) of OBPI is set
        if (CHECK_BIT(mmu->mem[OBPI], 7)) {
            byte_t new_obpi_address = (mmu->mem[OBPI] & 0x3F) + 1;
            if (new_obpi_address > 0x3F)
                new_obpi_address = 0;
            mmu->mem[OBPI] = new_obpi_address;
            SET_BIT(mmu->mem[OBPI], 7);
        }
    } else if (address == 0xFF74 && emu->mode == CGB) {
        mmu->mem[address] = data; // only writable in CGB mode
    } else if (address == 0xFF75 && emu->mode == CGB) {
        mmu->mem[address] = data & 0x70;
    } else if (address == 0xFF76 && emu->mode == CGB) {
        // read only
    } else if (address == 0xFF77 && emu->mode == CGB) {
        // read only
    } else {
        mmu->mem[address] = data;
    }
}
