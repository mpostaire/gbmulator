#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "emulator.h"
#include "mmu.h"
#include "ppu.h"
#include "joypad.h"
#include "apu.h"
#include "boot.h"
#include "cpu.h"
#include "link.h"

extern inline void rtc_update(rtc_t *rtc);

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
        mmu->has_eram = 1;
        mmu->has_battery = 1;
        break;
    case 0x0F:
        mmu->mbc = MBC3;
        mmu->has_rtc = 1;
        mmu->has_battery = 1;
        break;
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

    // 8-bit cartridge header checksum validation
    byte_t checksum = 0;
    for (int i = 0x0134; i <= 0x014C; i++)
        checksum = checksum - mmu->cartridge[i] - 1;
    if (checksum != mmu->cartridge[0x014D]) {
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

    return 1;
}

int mmu_init(emulator_t *emu, const byte_t *rom_data, size_t rom_size) {
    mmu_t *mmu = xcalloc(1, sizeof(mmu_t));
    mmu->current_rom_bank = 1;

    mmu->cartridge_size = rom_size;
    memcpy(mmu->cartridge, rom_data, mmu->cartridge_size);
    emu->mmu = mmu;

    if (parse_cartridge(emu)) {
        return 1;
    } else {
        mmu_quit(emu);
        return 0;
    }
}

void mmu_quit(emulator_t *emu) {
    free(emu->mmu);
}

static inline void oam_dma_step(mmu_t *mmu, emulator_mode_t mode) {
    // 4 cycles to transfer 1 byte

    if (mmu->oam_dma.src_address >= ROM_BANKN && mmu->oam_dma.src_address < VRAM) {
        mmu->mem[OAM + mmu->oam_dma.progress] = mmu->cartridge[(mmu->oam_dma.src_address - ROM_BANKN) + (mmu->current_rom_bank * 0x4000)];
    } else if (mmu->oam_dma.src_address >= ERAM && mmu->oam_dma.src_address < WRAM_BANK0) {
        mmu->mem[OAM + mmu->oam_dma.progress] = mmu->eram_enabled ? mmu->eram[(mmu->oam_dma.src_address - 0xA000) + (mmu->current_eram_bank * 0x2000)] : 0xFF;
    } else if (mode == CGB && mmu->oam_dma.src_address >= WRAM_BANKN && mmu->oam_dma.src_address < ECHO) {
        byte_t current_wram_bank = mmu->mem[SVBK] & 0x07;
        if (current_wram_bank == 0)
            current_wram_bank = 1;
        mmu->mem[OAM + mmu->oam_dma.progress] = mmu->wram_extra[(mmu->oam_dma.src_address - WRAM_BANKN) + ((current_wram_bank - 1) * 0x1000)];
    } else if (mmu->oam_dma.src_address >= ECHO && mmu->oam_dma.src_address < UNUSABLE) {
        word_t offset;
        if (mmu->oam_dma.src_address < OAM)
            offset = ECHO - WRAM_BANK0;
        else
            offset = OAM - WRAM_BANK0;

        if (mode == CGB) {
            byte_t current_wram_bank = mmu->mem[SVBK] & 0x07;
            if (current_wram_bank == 0)
                current_wram_bank = 1;
            word_t address = mmu->oam_dma.src_address - offset;
            mmu->mem[OAM + mmu->oam_dma.progress] = mmu->wram_extra[(address - WRAM_BANKN) + ((current_wram_bank - 1) * 0x1000)];
        } else {
            mmu->mem[OAM + mmu->oam_dma.progress] = mmu->mem[mmu->oam_dma.src_address - offset];
        }
    } else if (mmu->oam_dma.src_address >= IO && mmu->oam_dma.src_address < 0xFFFF) {
        // TODO doing nothing here doesn't pass mooneye acceptance/oam_dma/sources-GS test
        // mmu->mem[OAM + mmu->oam_dma.progress] = 0xFF;
    } else {
        mmu->mem[OAM + mmu->oam_dma.progress] = mmu->mem[mmu->oam_dma.src_address];
    }

    mmu->oam_dma.progress++;
    mmu->oam_dma.src_address++;
    mmu->oam_dma.is_active = mmu->oam_dma.progress != 0xA0;
}

static inline void hdma_gdma_step(emulator_t *emu, byte_t *src, byte_t *dest) {
    mmu_t *mmu = emu->mmu;
    mmu->hdma.step += 4; // 4 cycles per step

    // TODO handle cases where hdma.src_address goes from one region to another
    if (mmu->hdma.type == GDMA) {
        // 32 cycles to transfer 0x10 bytes
        if (mmu->hdma.step >= 32) {
            mmu->hdma.step = 0;

            // copy a block of 16 bytes from src to dest
            for (byte_t i = 0; i < 0x10; i++)
                dest[i] = src[i];

            // HDMA src and dest registers need to increase as the transfer progresses
            mmu->hdma.src_address += 0x10;
            // TODO src can't be inside vram, is this the correct way to handle this case?
            if (mmu->hdma.src_address >= VRAM && mmu->hdma.src_address < ERAM)
                mmu->hdma.src_address += ERAM - mmu->hdma.src_address;

            mmu->hdma.dest_address += 0x10;
            // vram overflow
            if (mmu->hdma.dest_address >= 0x2000) // 0x2000 is the size of a VRAM bank
                mmu->hdma.dest_address = 0x0000;

            mmu->mem[HDMA1] = mmu->hdma.src_address >> 8;
            mmu->mem[HDMA2] = mmu->hdma.src_address;
            mmu->mem[HDMA3] = mmu->hdma.dest_address >> 8;
            mmu->mem[HDMA4] = mmu->hdma.dest_address;

            mmu->mem[HDMA5]--;

            // transfer stops if mmu->mem[HDMA5] is 0xFF
            if (mmu->mem[HDMA5] == 0xFF)
                mmu->hdma.lock_cpu = 0;
        }
    } else if (CHECK_BIT(mmu->mem[LCDC], 7) && PPU_IS_MODE(emu, PPU_MODE_HBLANK) && mmu->hdma.hdma_ly == mmu->mem[LY]) {
        // TODO hdma still not perfect? pokemon crystal/zelda link's awakening dx have some visual glitches when displaying menus/text windows
        // TODO vram viewer like bgb to see what's happening in vram for these glitches to happen
        mmu->hdma.lock_cpu = 1;
        // 32 cycles to transfer 0x10 bytes
        if (mmu->hdma.step >= 32) {
            mmu->hdma.step = 0;

            mmu->hdma.hdma_ly++;
            if (mmu->hdma.hdma_ly == GB_SCREEN_HEIGHT)
                mmu->hdma.hdma_ly = 0;

            // copy a block of 16 bytes from src to dest
            for (byte_t i = 0; i < 0x10; i++)
                dest[i] = src[i];

            // HDMA src and dest registers need to increase as the transfer progresses
            mmu->hdma.src_address += 0x10;
            // TODO src can't be inside vram, is this the correct way to handle this case?
            if (mmu->hdma.src_address >= VRAM && mmu->hdma.src_address < ERAM)
                mmu->hdma.src_address += ERAM - mmu->hdma.src_address;

            mmu->hdma.dest_address += 0x10;
            // vram overflow
            if (mmu->hdma.dest_address >= 0x2000) // 0x2000 is the size of a VRAM bank
                mmu->hdma.dest_address = 0x0000;

            mmu->mem[HDMA1] = mmu->hdma.src_address >> 8;
            mmu->mem[HDMA2] = mmu->hdma.src_address;
            mmu->mem[HDMA3] = mmu->hdma.dest_address >> 8;
            mmu->mem[HDMA4] = mmu->hdma.dest_address;

            mmu->hdma.lock_cpu = 0;

            mmu->mem[HDMA5]--;
            // transfer stops if mmu->mem[HDMA5] is 0xFF
        }
    }
}

void mmu_step(emulator_t *emu) {
    mmu_t *mmu = emu->mmu;

    if (mmu->oam_dma.is_active)
        oam_dma_step(mmu, emu->mode);

    // do GDMA/HDMA if in CGB mode and HDMA5 bit 7 is reset (meaning there is an active HDMA/GDMA) 
    if (emu->mode == CGB && !CHECK_BIT(mmu->mem[HDMA5], 7)) {
        // get src memory area pointer

        // TODO handle cases where hdma.src_address goes from one region to another
        byte_t *src;
        if (mmu->hdma.src_address >= ROM_BANKN && mmu->hdma.src_address < VRAM) {
            src = &mmu->cartridge[(mmu->hdma.src_address - ROM_BANKN) + (mmu->current_rom_bank * 0x4000)];
        } else if (mmu->eram_enabled && mmu->oam_dma.src_address >= ERAM && mmu->oam_dma.src_address < WRAM_BANK0) {
            // TODO check if this is accurate
            src = &mmu->eram[(mmu->oam_dma.src_address - 0xA000) + (mmu->current_eram_bank * 0x2000)];
        } else if (mmu->hdma.src_address >= WRAM_BANKN && mmu->hdma.src_address < ECHO) {
            byte_t current_wram_bank = mmu->mem[SVBK] & 0x07;
            if (current_wram_bank == 0)
                current_wram_bank = 1;
            src = &mmu->wram_extra[(mmu->hdma.src_address - WRAM_BANKN) + ((current_wram_bank - 1) * 0x1000)];
        } else {
            src = &mmu->mem[mmu->hdma.src_address];
        }

        // get dest memory area pointer
        byte_t *dest = CHECK_BIT(mmu->mem[VBK], 0) ? &mmu->vram_extra[mmu->hdma.dest_address] : &mmu->mem[VRAM + mmu->hdma.dest_address];

        hdma_gdma_step(emu, src, dest);
    }
}

// TODO MBC1 special cases where rom bank 0 is changed are not implemented
static void write_mbc_registers(mmu_t *mmu, word_t address, byte_t data) {
    switch (mmu->mbc) {
    case ROM_ONLY:
        break; // read only, do nothing
    case MBC1:
        if (address < 0x2000) {
            mmu->eram_enabled = mmu->has_eram && (data & 0x0F) == 0x0A;
        } else if (address < 0x4000) {
            data &= 0x1F;
            if (data == 0x00)
                data = 0x01;

            if (mmu->mbc1_mode == 0)
                mmu->current_rom_bank = (mmu->mbc_rom_bank_hi << 5) | data;
            else
                mmu->current_rom_bank = data;

            // printf("mode=%d requested: %d (%02X), got: %d (%02X) --- %d\n", mmu->mbc1_mode, data, data, mmu->current_rom_bank, mmu->current_rom_bank, mmu->mbc_rom_bank_hi);

            mmu->current_rom_bank &= mmu->rom_banks - 1; // in this case, equivalent to current_rom_bank %= rom_banks but avoid division by 0
        } else if (address < 0x6000) {
            data &= 0x03;
            if (mmu->mbc1_mode == 0) { // ROM mode
                mmu->mbc_rom_bank_hi = data;
                mmu->current_rom_bank = (mmu->mbc_rom_bank_hi << 5) | (mmu->current_rom_bank & 0x1F);
                // TODO commenting this passes more tests but I think its necessary to keep it so the problem may come from the read instead of the write?
                // if (mmu->current_rom_bank == 0x00 || mmu->current_rom_bank == 0x20 || mmu->current_rom_bank == 0x40 || mmu->current_rom_bank == 0x60)
                //     mmu->current_rom_bank++;
                mmu->current_rom_bank &= mmu->rom_banks - 1; // in this case, equivalent to current_rom_bank %= rom_banks but avoid division by 0
                mmu->current_eram_bank = 0x00;
            } else {
                mmu->current_eram_bank = data;
                mmu->current_eram_bank &= mmu->eram_banks - 1; // in this case, equivalent to current_eram_bank %= eram_banks but avoid division by 0
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
                rtc_update(&mmu->rtc);
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
            mmu->eram[(address - ERAM) + (mmu->current_eram_bank * 0x2000)] = data;
        break;
    case MBC2:
        if (mmu->has_eram && mmu->eram_enabled)
            mmu->eram[(address - ERAM) + (mmu->current_eram_bank * 0x0200)] = data;
        break;
    case MBC3:
    case MBC30:
        if (mmu->has_eram && mmu->eram_enabled && mmu->current_eram_bank >= 0) {
            mmu->eram[(address - ERAM) + (mmu->current_eram_bank * 0x2000)] = data;
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
            mmu->eram[(address - ERAM) + (mmu->current_eram_bank * 0x2000)] = data;
        // TODO MBC5 eram is the same as for MBC1, except that RAM sizes are 8 KiB, 32 KiB and 128 KiB.
        break;
    default:
        break;
    }
}

byte_t mmu_read(emulator_t *emu, word_t address) {
    mmu_t *mmu = emu->mmu;

    if (mmu->oam_dma.is_active)
        return (address >= HRAM && address < IE) ? mmu->mem[address] : 0xFF;

    if (mmu->mbc >= MBC1 && mmu->mbc <= MBC5) {
        if (address >= ROM_BANKN && address < VRAM) // ROM_BANKN
            return mmu->cartridge[(address - ROM_BANKN) + (mmu->current_rom_bank * 0x4000)];

        if (address >= ERAM && address < WRAM_BANK0) { // ERAM
            if (mmu->has_eram && mmu->current_eram_bank >= 0) {
                if (mmu->mbc == MBC2) {
                    address = ERAM + (address & 0x1FF); // wrap around from 0xA200 to 0xBFFF (eg: address 0xA200 reads as address 0xA000)
                    return mmu->eram_enabled ? mmu->eram[(address - ERAM) + (mmu->current_eram_bank * 0x0200)] | 0xF0 : 0xFF;
                } else {
                    return mmu->eram_enabled ? mmu->eram[(address - ERAM) + (mmu->current_eram_bank * 0x2000)] : 0xFF;
                }
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

    if (address >= ECHO && address < OAM) {
        if (emu->mode == CGB) {
            byte_t current_wram_bank = mmu->mem[SVBK] & 0x07;
            if (current_wram_bank == 0)
                current_wram_bank = 1;
            address -= ECHO - WRAM_BANK0;
            return mmu->wram_extra[(address - WRAM_BANKN) + ((current_wram_bank - 1) * 0x1000)];
        } else {
            return mmu->mem[address - (ECHO - WRAM_BANK0)];
        }
    }

    // UNUSABLE memory is unusable
    if (address >= UNUSABLE && address < IO)
        return 0xFF;

    // Reading from P1 register returns joypad input state according to its current bit 4 or 5 value
    if (address == P1)
        return joypad_get_input(emu);

    if (address == SC)
        return mmu->mem[address] | (emu->mode == CGB ? 0x7C : 0x7E);

    if (address == DIV_LSB)
        return 0xFF;
    
    if (address == TAC)
        return mmu->mem[address] | 0xF8;

    if (address >= 0xFF08 && address < IF)
        return 0xFF;

    if (address == IF)
        return mmu->mem[IF] | 0xE0;

    if (address == NR10)
        return mmu->mem[NR10] | 0x80;
    if (address == NR11)
        return mmu->mem[NR11] | 0x3F;
    // if (address == NR12)
    //     return mmu->mem[NR12] | 0x00; // useless
    if (address == NR13)
        return 0xFF;
    if (address == NR14)
        return mmu->mem[NR14] | 0xBF;

    if (address == NR20)
        return 0xFF;
    if (address == NR21)
        return mmu->mem[NR21] | 0x3F;
    // if (address == NR22)
    //     return mmu->mem[NR22] | 0x00; // useless
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
    // if (address == NR42)
    //     return mmu->mem[NR42] | 0x00; // useless
    // if (address == NR43)
    //     return mmu->mem[NR43] | 0x00; // useless
    if (address == NR44)
        return mmu->mem[NR44] | 0xBF;

    // if (address == NR50)
    //     return mmu->mem[NR50] | 0x00; // useless
    // if (address == NR51)
    //     return mmu->mem[NR51] | 0x00; // useless
    if (address == NR52)
        return mmu->mem[NR52] | 0x70;
    if (address > NR52 && address < WAVE_RAM)
        return 0xFF;
    
    if (address == KEY0 || address == KEY1)
        return emu->mode == CGB ? mmu->mem[address] : 0xFF;

    if (emu->mode == CGB) {
        if (address == 0xFF75)
            return mmu->mem[address] & 0x70;

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
    } else {
        if (address > KEY1 && address < HRAM)
            return 0xFF;
    }

    return mmu->mem[address];
}

void mmu_write(emulator_t* emu, word_t address, byte_t data) {
    mmu_t *mmu = emu->mmu;

    if (mmu->oam_dma.is_active) {
        if (address >= HRAM && address < IE)
            mmu->mem[address] = data;
        return;
    }

    if (address < VRAM) {
        write_mbc_registers(mmu, address, data);
    } else if (/*address >= VRAM &&*/ address < ERAM) {
        // VRAM inaccessible by cpu while ppu in mode 3 and LCD is enabled (return undefined data)
        if (CHECK_BIT(mmu->mem[LCDC], 7) && PPU_IS_MODE(emu, PPU_MODE_DRAWING))
            return;

        if (emu->mode == CGB && CHECK_BIT(mmu->mem[VBK], 0))
            mmu->vram_extra[(address - VRAM)] = data;
        else
            mmu->mem[address] = data;
    } else if (/*address >= ERAM &&*/ address < WRAM_BANK0) {
        write_mbc_eram(mmu, address, data);
    } else if (emu->mode == CGB && address >= WRAM_BANKN && address < ECHO) {
        byte_t current_wram_bank = mmu->mem[SVBK] & 0x07;
        if (current_wram_bank == 0)
            current_wram_bank = 1;
        mmu->wram_extra[(address - WRAM_BANKN) + ((current_wram_bank - 1) * 0x1000)] = data;
    } else if (address > ECHO && address <= OAM){
        if (emu->mode == CGB) {
            byte_t current_wram_bank = mmu->mem[SVBK] & 0x07;
            if (current_wram_bank == 0)
                current_wram_bank = 1;
            address -= ECHO - WRAM_BANK0;
            mmu->wram_extra[(address - WRAM_BANKN) + ((current_wram_bank - 1) * 0x1000)] = data;
        } else {
            mmu->mem[address - (ECHO - WRAM_BANK0)] = data;
        }
    } else if ((address >= OAM && address < UNUSABLE) && ((PPU_IS_MODE(emu, PPU_MODE_OAM) || PPU_IS_MODE(emu, PPU_MODE_DRAWING)) && CHECK_BIT(mmu->mem[LCDC], 7))) {
        // OAM inaccessible by cpu while ppu in mode 2 or 3 and LCD enabled
    } else if (address >= UNUSABLE && address < IO) {
        // UNUSABLE memory is unusable
    } else if (address == DIV_LSB) {
        // writing to DIV resets it to 0
        mmu->mem[address] = 0;
        mmu->mem[DIV] = 0;
        mmu->mem[TIMA] = mmu->mem[TMA];
    } else if (address == DIV) {
        // writing to DIV resets it to 0
        mmu->mem[address] = 0;
        mmu->mem[DIV_LSB] = 0;
        mmu->mem[TIMA] = mmu->mem[TMA];
    } else if (address == TAC) {
        mmu->mem[address] = data;
        switch (data & 0x03) {
        case 0: emu->timer->max_tima_cycles = 1024; break;
        case 1: emu->timer->max_tima_cycles = 16; break;
        case 2: emu->timer->max_tima_cycles = 64; break;
        case 3: emu->timer->max_tima_cycles = 256; break;
        }
    } else if (address == LY) {
        // read only
    } else if (address == LYC) {
        // a write to LYC triggers an immediate LY=LYC comparison
        mmu->mem[address] = data;
        ppu_ly_lyc_compare(emu);
    } else if (address == DMA) {
        // writing to this register starts an OAM DMA transfer
        mmu->oam_dma.is_active = 1;
        mmu->oam_dma.progress = 0;
        mmu->oam_dma.src_address = data * 0x100;

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

        if (!CHECK_BIT(mmu->mem[HDMA5], 7) && mmu->hdma.type == HDMA && GET_BIT(data, 7))
            eprintf("TODO cancel HDMA\n"); // TODO cancel HDMA at next HBLANK

        mmu->hdma.type = GET_BIT(data, 7);
        mmu->hdma.lock_cpu = mmu->hdma.type == GDMA || (mmu->hdma.type == HDMA && PPU_IS_MODE(emu, PPU_MODE_HBLANK));

        // bit 7 of HDMA5 is 0 to show that there is an active HDMA/GDMA, bits 0-6 are the size of the DMA
        mmu->mem[address] = data & 0x7F;

        mmu->hdma.step = 31;
        if (mmu->hdma.type == HDMA) {
            mmu->hdma.hdma_ly = mmu->mem[LY] + 1; // start HDMA at next HBLANK
            if (mmu->hdma.hdma_ly == GB_SCREEN_HEIGHT)
                mmu->hdma.hdma_ly = 0;
        }

        mmu->hdma.src_address = ((mmu->mem[HDMA1] << 8) | mmu->mem[HDMA2]) & 0xFFF0;
        mmu->hdma.dest_address = ((mmu->mem[HDMA3] << 8) | mmu->mem[HDMA4]) & 0x1FF0;

        // if (mmu->hdma.type) { // HBLANK DMA (HDMA)
        //     printf("HDMA size=%d, vram bank=%d, src=%x, dest=%x\n", (mmu->mem[address] + 1) * 0x10, mmu->mem[VBK] & 0x01, mmu->hdma.src_address, mmu->hdma.dest_address + VRAM);
        // } else { // General purpose DMA (GDMA)
        //     printf("GDMA size=%d, vram bank=%d, src=%x, dest=%x\n", (mmu->mem[address] + 1) * 0x10, mmu->mem[VBK] & 0x01, mmu->hdma.src_address, mmu->hdma.dest_address + VRAM);
        // }
    } else if (address == P1) {
        // prevent writes to the lower nibble of the P1 register (joypad)
        mmu->mem[address] = data & 0xF0;
    } else if (address == SC) {
        if (CHECK_BIT(mmu->mem[SC], 1) != CHECK_BIT(data, 1)) {
            mmu->mem[address] = data & 0x83;
            link_set_clock(emu);
        } else {
            mmu->mem[address] = data & 0x83;
        }
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

size_t mmu_serialized_length(emulator_t *emu) {
    size_t mem_len = sizeof(emu->mmu->mem);
    size_t eram_len = 0x2000 * emu->mmu->eram_banks;
    size_t wram_extra_len = 0;
    size_t vram_extra_len = 0;
    size_t cram_len = 0;
    size_t oam_dma_len = 5;
    size_t hdma_len = 0;
    size_t rtc_len = 13 + (2 * time_to_string_length());
    size_t mmu_other_len = 15;
    if (emu->mode == CGB) {
        wram_extra_len = sizeof(emu->mmu->wram_extra);
        vram_extra_len = sizeof(emu->mmu->vram_extra);
        cram_len = sizeof(emu->mmu->cram_bg) + sizeof(emu->mmu->cram_obj);
        hdma_len = 8;
    }
    return mem_len + mmu_other_len + eram_len + wram_extra_len + vram_extra_len + cram_len + hdma_len + oam_dma_len + rtc_len;
}

byte_t *mmu_serialize(emulator_t *emu, size_t *size) {
    *size = mmu_serialized_length(emu);
    byte_t *buf = xmalloc(*size);

    size_t offset = 0;
    memcpy(buf, emu->mmu->mem, sizeof(emu->mmu->mem));
    offset += sizeof(emu->mmu->mem);

    size_t eram_len = 0x2000 * emu->mmu->eram_banks;
    memcpy(&buf[offset], emu->mmu->eram, eram_len);
    offset += eram_len;

    if (emu->mode == CGB) {
        size_t wram_extra_len = sizeof(emu->mmu->wram_extra);
        memcpy(&buf[offset], emu->mmu->wram_extra, wram_extra_len);
        offset += wram_extra_len;

        size_t vram_extra_len = sizeof(emu->mmu->vram_extra);
        memcpy(&buf[offset], emu->mmu->vram_extra, vram_extra_len);
        offset += vram_extra_len;

        size_t cram_bg_len = sizeof(emu->mmu->cram_bg);
        memcpy(&buf[offset], emu->mmu->cram_bg, cram_bg_len);
        offset += cram_bg_len;
        size_t cram_obj_len = sizeof(emu->mmu->cram_obj);
        memcpy(&buf[offset], emu->mmu->cram_obj, cram_obj_len);
        offset += cram_obj_len;

        size_t hdma_len = 8;
        memcpy(&buf[offset], &emu->mmu->hdma.step, 1);
        memcpy(&buf[offset + 1], &emu->mmu->hdma.hdma_ly, 1);
        memcpy(&buf[offset + 2], &emu->mmu->hdma.lock_cpu, 1);
        memcpy(&buf[offset + 3], &emu->mmu->hdma.type, 1);
        memcpy(&buf[offset + 4], &emu->mmu->hdma.src_address, 2);
        memcpy(&buf[offset + 6], &emu->mmu->hdma.dest_address, 2);
        offset += hdma_len;
    }

    memcpy(&buf[offset], &emu->mmu->oam_dma.is_active, 1);
    memcpy(&buf[offset + 1], &emu->mmu->oam_dma.progress, 2);
    memcpy(&buf[offset + 3], &emu->mmu->oam_dma.src_address, 2);
    offset += 5;

    memcpy(&buf[offset], &emu->mmu->mbc, 1);
    memcpy(&buf[offset + 1], &emu->mmu->rom_banks, 2);
    memcpy(&buf[offset + 3], &emu->mmu->eram_banks, 1);
    memcpy(&buf[offset + 4], &emu->mmu->current_rom_bank, 2);
    memcpy(&buf[offset + 6], &emu->mmu->mbc_rom_bank_hi, 1);
    memcpy(&buf[offset + 7], &emu->mmu->current_eram_bank, 2);
    memcpy(&buf[offset + 9], &emu->mmu->mbc1_mode, 1);
    memcpy(&buf[offset + 10], &emu->mmu->eram_enabled, 1);
    memcpy(&buf[offset + 11], &emu->mmu->has_eram, 1);
    memcpy(&buf[offset + 12], &emu->mmu->has_battery, 1);
    memcpy(&buf[offset + 13], &emu->mmu->has_rumble, 1);
    memcpy(&buf[offset + 14], &emu->mmu->has_rtc, 1);
    offset += 15;

    memcpy(&buf[offset], &emu->mmu->rtc.latched_s, 1);
    memcpy(&buf[offset + 1], &emu->mmu->rtc.latched_m, 1);
    memcpy(&buf[offset + 2], &emu->mmu->rtc.latched_h, 1);
    memcpy(&buf[offset + 3], &emu->mmu->rtc.latched_dl, 1);
    memcpy(&buf[offset + 4], &emu->mmu->rtc.latched_dh, 1);
    memcpy(&buf[offset + 5], &emu->mmu->rtc.s, 1);
    memcpy(&buf[offset + 6], &emu->mmu->rtc.m, 1);
    memcpy(&buf[offset + 7], &emu->mmu->rtc.h, 1);
    memcpy(&buf[offset + 8], &emu->mmu->rtc.dl, 1);
    memcpy(&buf[offset + 9], &emu->mmu->rtc.dh, 1);
    memcpy(&buf[offset + 10], &emu->mmu->rtc.enabled, 1);
    memcpy(&buf[offset + 11], &emu->mmu->rtc.reg, 1);
    memcpy(&buf[offset + 12], &emu->mmu->rtc.latch, 1);
    offset += 13;

    size_t len;
    char *str = time_to_string(emu->mmu->rtc.value_in_seconds, &len);
    memcpy(&buf[offset], str, len);
    offset += len;
    free(str);
    str = time_to_string(emu->mmu->rtc.timestamp, &len);
    memcpy(&buf[offset], str, len);
    offset += len;
    free(str);

    // TODO remove this check
    if (offset != *size) {
        eprintf("offset != size (%zu != %zu)\n", offset, *size);
        exit(42);
    }

    return buf;
}

void mmu_unserialize(emulator_t *emu, byte_t *buf) {
    size_t offset = 0;
    memcpy(emu->mmu->mem, buf, sizeof(emu->mmu->mem));
    offset += sizeof(emu->mmu->mem);

    size_t eram_len = 0x2000 * emu->mmu->eram_banks;
    memcpy(emu->mmu->eram, &buf[offset], eram_len);
    offset += eram_len;

    if (emu->mode == CGB) {
        size_t wram_extra_len = sizeof(emu->mmu->wram_extra);
        memcpy(emu->mmu->wram_extra, &buf[offset], wram_extra_len);
        offset += wram_extra_len;

        size_t vram_extra_len = sizeof(emu->mmu->vram_extra);
        memcpy(emu->mmu->vram_extra, &buf[offset], vram_extra_len);
        offset += vram_extra_len;

        size_t cram_bg_len = sizeof(emu->mmu->cram_bg);
        memcpy(emu->mmu->cram_bg, &buf[offset], cram_bg_len);
        offset += cram_bg_len;
        size_t cram_obj_len = sizeof(emu->mmu->cram_obj);
        memcpy(emu->mmu->cram_obj, &buf[offset], cram_obj_len);
        offset += cram_obj_len;

        size_t hdma_len = 8;
        memcpy(&emu->mmu->hdma.step, &buf[offset], 1);
        memcpy(&emu->mmu->hdma.hdma_ly, &buf[offset + 1], 1);
        memcpy(&emu->mmu->hdma.lock_cpu, &buf[offset + 2], 1);
        memcpy(&emu->mmu->hdma.type, &buf[offset + 3], 1);
        memcpy(&emu->mmu->hdma.src_address, &buf[offset + 4], 2);
        memcpy(&emu->mmu->hdma.dest_address, &buf[offset + 6], 2);
        offset += hdma_len;
    }

    memcpy(&emu->mmu->oam_dma.is_active, &buf[offset], 1);
    memcpy(&emu->mmu->oam_dma.progress, &buf[offset + 1], 2);
    memcpy(&emu->mmu->oam_dma.src_address, &buf[offset + 3], 2);
    offset += 5;

    memcpy(&emu->mmu->mbc, &buf[offset], 1);
    memcpy(&emu->mmu->rom_banks, &buf[offset + 1], 2);
    memcpy(&emu->mmu->eram_banks, &buf[offset + 3], 1);
    memcpy(&emu->mmu->current_rom_bank, &buf[offset + 4], 2);
    memcpy(&emu->mmu->mbc_rom_bank_hi, &buf[offset + 6], 1);
    memcpy(&emu->mmu->current_eram_bank, &buf[offset + 7], 2);
    memcpy(&emu->mmu->mbc1_mode, &buf[offset + 9], 1);
    memcpy(&emu->mmu->eram_enabled, &buf[offset + 10], 1);
    memcpy(&emu->mmu->has_eram, &buf[offset + 11], 1);
    memcpy(&emu->mmu->has_battery, &buf[offset + 12], 1);
    memcpy(&emu->mmu->has_rumble, &buf[offset + 13], 1);
    memcpy(&emu->mmu->has_rtc, &buf[offset + 14], 1);
    offset += 15;

    memcpy(&emu->mmu->rtc.latched_s, &buf[offset], 1);
    memcpy(&emu->mmu->rtc.latched_m, &buf[offset + 1], 1);
    memcpy(&emu->mmu->rtc.latched_h, &buf[offset + 2], 1);
    memcpy(&emu->mmu->rtc.latched_dl, &buf[offset + 3], 1);
    memcpy(&emu->mmu->rtc.latched_dh, &buf[offset + 4], 1);
    memcpy(&emu->mmu->rtc.s, &buf[offset + 5], 1);
    memcpy(&emu->mmu->rtc.m, &buf[offset + 6], 1);
    memcpy(&emu->mmu->rtc.h, &buf[offset + 7], 1);
    memcpy(&emu->mmu->rtc.dl, &buf[offset + 8], 1);
    memcpy(&emu->mmu->rtc.dh, &buf[offset + 9], 1);
    memcpy(&emu->mmu->rtc.enabled, &buf[offset + 10], 1);
    memcpy(&emu->mmu->rtc.reg, &buf[offset + 11], 1);
    memcpy(&emu->mmu->rtc.latch, &buf[offset + 12], 1);
    offset += 13;

    emu->mmu->rtc.value_in_seconds = string_to_time((char *) &buf[offset]);
    emu->mmu->rtc.timestamp = string_to_time((char *) &buf[offset + time_to_string_length()]);
}
