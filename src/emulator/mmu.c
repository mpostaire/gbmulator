#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "emulator_priv.h"
#include "boot.h"
#include "mbc.h"

#define GBC_CURRENT_VRAM_BANK(mmu) ((mmu)->io_registers[VBK - IO] & 0x01)
#define GBC_CURRENT_WRAM_BANK(mmu) (((mmu)->io_registers[SVBK - IO] & 0x07) == 0 ? 1 : ((mmu)->io_registers[SVBK - IO] & 0x07))

static int parse_cartridge(emulator_t *emu) {
    mmu_t *mmu = emu->mmu;

    byte_t has_eram = 0;
    switch (mmu->rom[0x0147]) {
    case 0x00:
        mmu->mbc.type = ROM_ONLY;
        break;
    case 0x01:
        mmu->mbc.type = MBC1;
        break;
    case 0x02:
        mmu->mbc.type = MBC1;
        has_eram = 1;
        break;
    case 0x03:
        mmu->mbc.type = MBC1;
        has_eram = 1;
        mmu->has_battery = 1;
        break;
    case 0x05:
        mmu->mbc.type = MBC2;
        break;
    case 0x06:
        mmu->mbc.type = MBC2;
        has_eram = 1;
        mmu->has_battery = 1;
        break;
    case 0x0F:
        mmu->mbc.type = MBC3;
        mmu->has_rtc = 1;
        mmu->has_battery = 1;
        break;
    case 0x10:
        mmu->mbc.type = MBC3;
        mmu->has_rtc = 1;
        mmu->has_battery = 1;
        has_eram = 1;
        break;
    case 0x11:
        mmu->mbc.type = MBC3;
        break;
    case 0x12:
        mmu->mbc.type = MBC3;
        has_eram = 1;
        break;
    case 0x13:
        mmu->mbc.type = MBC3;
        mmu->has_battery = 1;
        has_eram = 1;
        break;
    case 0x19:
        mmu->mbc.type = MBC5;
        break;
    case 0x1A:
        mmu->mbc.type = MBC5;
        has_eram = 1;
        break;
    case 0x1B:
        mmu->mbc.type = MBC5;
        has_eram = 1;
        mmu->has_battery = 1;
        break;
    case 0x1C:
        mmu->mbc.type = MBC5;
        mmu->has_rumble = 1;
        break;
    case 0x1D:
        mmu->mbc.type = MBC5;
        mmu->has_rumble = 1;
        has_eram = 1;
        break;
    case 0x1E:
        mmu->mbc.type = MBC5;
        mmu->has_rumble = 1;
        has_eram = 1;
        mmu->has_battery = 1;
        break;
    // case 0x20:
    //     mmu->mbc.type = MBC6;
    //     break;
    case 0x22:
        mmu->mbc.type = MBC7;
        break;
    // case 0xFC:
    //     mmu->mbc.type = CAMERA;
    //     break;
    // case 0xFD:
    //     mmu->mbc.type = TAMA5;
    //     break;
    case 0xFE:
        mmu->mbc.type = HuC3;
        mmu->has_rtc = 1;
        has_eram = 1;
        mmu->has_battery = 1;
        break;
    case 0xFF:
        mmu->mbc.type = HuC1;
        has_eram = 1;
        mmu->has_battery = 1;
        break;
    default:
        eprintf("MBC byte %02X not supported\n", mmu->rom[0x0147]);
        return 0;
    }

    // detect MBC1M
    if (mmu->mbc.type == MBC1 && mmu->rom_size == 0x100000) {
        const unsigned int addrs[] = { 0x00104, 0x40104, 0x80104, 0xC0104 };
        byte_t logo[] = {
            0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
            0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E, 0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
            0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E
        };

        byte_t matches = 0;
        for (byte_t i = 0; i < 4; i++) {
            matches += memcmp(&mmu->rom[addrs[i]], logo, sizeof(logo)) ? 0 : 1;
            if (matches > 1) {
                mmu->mbc.type = MBC1M;
                break;
            }
        }
    }

    mmu->rom_banks = 2 << mmu->rom[0x0148];

    if (has_eram) {
        switch (mmu->rom[0x0149]) {
        case 0x00: mmu->eram_banks = 0; break;
        case 0x02: mmu->eram_banks = 1; break;
        case 0x03: mmu->eram_banks = 4; break;
        case 0x04: mmu->eram_banks = 16; break;
        case 0x05: mmu->eram_banks = 8; break;
        }
    }

    // MBC3 cartridges are 2MiB, MBC30 cartridges are 4MiB (but the mbctest.gb test rom is a bit smaller)
    if (mmu->mbc.type == MBC3)
        if (mmu->eram_banks == 8 || mmu->rom_size > 0x00200000)
            mmu->mbc.type = MBC30;

    // get rom title
    memcpy(emu->rom_title, (char *) &mmu->rom[0x0134], 16);
    emu->rom_title[16] = '\0';
    byte_t cgb_flag = mmu->rom[0x0143] == 0xC0 || mmu->rom[0x0143] == 0x80;
    if (cgb_flag)
        emu->rom_title[15] = '\0';

    // remove trailing non alphanumeric characters from the rom title
    for (char *c = &emu->rom_title[16]; c >= emu->rom_title; c--) {
        if (isalnum(*c))
            break;
        *c = '\0';
    }

    // TODO better understand and implement CGB's DMG compatbility mode
    // byte_t cgb_dmg_compat = emu->mode == CGB && !cgb_flag;
    // byte_t cgb_mode_enabled = emu->mode == CGB && cgb_flag;

    // 8-bit cartridge header checksum validation
    byte_t checksum = 0;
    for (int i = 0x0134; i <= 0x014C; i++)
        checksum = checksum - mmu->rom[i] - 1;
    if (checksum != mmu->rom[0x014D]) {
        eprintf("invalid checksum\n");
        return 0;
    }

    return 1;
}

int mmu_init(emulator_t *emu, const byte_t *rom_data, size_t rom_size) {
    mmu_t *mmu = xcalloc(1, sizeof(mmu_t));
    mmu->rom = xcalloc(1, rom_size);

    mmu->mbc.mbc1.bank_lo = 1;
    // mmu->rom_bank0_addr = 0; // initialized to 0 by xcalloc
    // mmu->rom_bankn_addr = 0; // initialized to ROM_BANKN - ROM_BANK_SIZE = 0 by xcalloc
    // mmu->eram_bank_addr = 0; // initialized to 0 by xcalloc
    mmu->wram_bankn_addr_offset = -WRAM_BANK0;
    mmu->vram_bank_addr_offset = -VRAM;

    // mmu->mbc7.accelerometer.latched_x = 0x8000;
    // mmu->mbc7.accelerometer.latched_y = 0x8000;
    // mmu->mbc7.accelerometer.latch_ready = 1;

    mmu->rom_size = rom_size;
    memcpy(mmu->rom, rom_data, mmu->rom_size);
    emu->mmu = mmu;

    if (parse_cartridge(emu))
        return 1;

    mmu_quit(emu);
    return 0;
}

void mmu_quit(emulator_t *emu) {
    free(emu->mmu->rom);
    free(emu->mmu);
}

static inline byte_t read_io_register(emulator_t *emu, word_t address) {
    mmu_t *mmu = emu->mmu;
    word_t io_reg_addr = address - IO;

    switch (address) {
    case P1:
        // Reading from P1 register returns joypad input state according to its current bit 4 or 5 value
        return joypad_get_input(emu);
    case SB: return mmu->io_registers[io_reg_addr];
    case SC: return mmu->io_registers[io_reg_addr] | (emu->mode == CGB ? 0x7C : 0x7E);
    case DIV_LSB: return 0xFF;
    case DIV: return mmu->io_registers[io_reg_addr];
    case TIMA: return mmu->io_registers[io_reg_addr];
    case TMA: return mmu->io_registers[io_reg_addr];
    case TAC: return mmu->io_registers[io_reg_addr] | 0xF8;
    case 0xFF08 ... IF - 1: return 0xFF;
    case IF: return mmu->io_registers[io_reg_addr] | 0xE0;
    case NR10: return mmu->io_registers[io_reg_addr] | 0x80;
    case NR11: return mmu->io_registers[io_reg_addr] | 0x3F;
    case NR12: return mmu->io_registers[io_reg_addr] | 0x00; // useless bitwise or
    case NR13: return 0xFF;
    case NR14: return mmu->io_registers[io_reg_addr] | 0xBF;
    case NR20: return 0xFF;
    case NR21: return mmu->io_registers[io_reg_addr] | 0x3F;
    case NR22: return mmu->io_registers[io_reg_addr] | 0x00; // useless bitwise or
    case NR23: return 0xFF;
    case NR24: return mmu->io_registers[io_reg_addr] | 0xBF;
    case NR30: return mmu->io_registers[io_reg_addr] | 0x7F;
    case NR31: return 0xFF;
    case NR32: return mmu->io_registers[io_reg_addr] | 0x9F;
    case NR33: return 0xFF;
    case NR34: return mmu->io_registers[io_reg_addr] | 0xBF;
    case NR40: return 0xFF;
    case NR41: return 0xFF;
    case NR42: return mmu->io_registers[io_reg_addr] | 0x00; // useless bitwise or
    case NR43: return mmu->io_registers[io_reg_addr] | 0x00; // useless bitwise or
    case NR44: return mmu->io_registers[io_reg_addr] | 0xBF;
    case NR50: return mmu->io_registers[io_reg_addr] | 0x00; // useless bitwise or
    case NR51: return mmu->io_registers[io_reg_addr] | 0x00; // useless bitwise or
    case NR52: return mmu->io_registers[io_reg_addr] | 0x70;
    case 0xFF27 ... WAVE_RAM - 1: return 0xFF;
    case WAVE_RAM ... LCDC - 1: return mmu->io_registers[io_reg_addr];
    case LCDC: return mmu->io_registers[io_reg_addr];
    case STAT: return mmu->io_registers[io_reg_addr] | 0x80;
    case SCY: return mmu->io_registers[io_reg_addr];
    case SCX: return mmu->io_registers[io_reg_addr];
    case LY: return mmu->io_registers[io_reg_addr];
    case LYC: return mmu->io_registers[io_reg_addr];
    case DMA: return mmu->io_registers[io_reg_addr];
    case BGP: return mmu->io_registers[io_reg_addr];
    case OBP0: return mmu->io_registers[io_reg_addr];
    case OBP1: return mmu->io_registers[io_reg_addr];
    case WY: return mmu->io_registers[io_reg_addr];
    case WX: return mmu->io_registers[io_reg_addr];
    case KEY0: return emu->mode == CGB ? mmu->io_registers[io_reg_addr] : 0xFF;
    case KEY1: return emu->mode == CGB ? (mmu->io_registers[io_reg_addr] | 0x7E) : 0xFF;
    case 0xFF4E: return 0xFF;
    case VBK: return emu->mode == CGB ? 0xFE | (mmu->io_registers[io_reg_addr] & 0x01) : 0xFF;
    case BANK: return 0xFF;
    case HDMA1 ... HDMA4: return 0xFF;
    case HDMA5: return emu->mode == CGB ? mmu->io_registers[io_reg_addr] : 0xFF;
    case RP: return emu->mode == CGB ? 0x3E : 0xFF; // TODO GBC infrared LED
    case 0xFF57 ... 0xFF67: return 0xFF;
    case BGPI: return emu->mode == CGB ? mmu->io_registers[io_reg_addr] : 0xFF;
    case BGPD: {
        if (emu->mode == DMG || PPU_IS_MODE(emu, PPU_MODE_DRAWING))
            return 0xFF;

        byte_t cram_address = mmu->io_registers[BGPI - IO] & 0x3F;
        return mmu->cram_bg[cram_address];
    }
    case OBPI: return emu->mode == CGB ? mmu->io_registers[io_reg_addr] : 0xFF;
    case OBPD: {
        if (emu->mode == DMG || PPU_IS_MODE(emu, PPU_MODE_DRAWING))
            return 0xFF;

        byte_t cram_address = mmu->io_registers[OBPI - IO] & 0x3F;
        return mmu->cram_obj[cram_address];
    }
    case 0xFF6C ... 0xFF6F: return 0xFF;
    case SVBK: return emu->mode == CGB ? 0xF8 | (mmu->io_registers[io_reg_addr] & 0x07) : 0xFF;
    case 0xFF71 ... 0xFF74: return 0xFF;
    case 0xFF75: return emu->mode == CGB ? mmu->io_registers[io_reg_addr] & 0x70 : 0xFF;
    case 0xFF76 ... 0xFF7F: return 0xFF;
    default:
        eprintf("invalid read at 0x%04X\n", address);
        exit(EXIT_FAILURE);
    }
}

static inline void write_io_register(emulator_t *emu, word_t address, byte_t data) {
    mmu_t *mmu = emu->mmu;
    word_t io_reg_addr = address - IO;

    switch (address) {
    case P1:
        // prevent writes to the lower nibble of the P1 register (joypad)
        mmu->io_registers[io_reg_addr] = data & 0xF0;
        break;
    case SC:
        if (CHECK_BIT(mmu->io_registers[io_reg_addr], 1) != CHECK_BIT(data, 1)) {
            mmu->io_registers[io_reg_addr] = data & 0x83;
            link_set_clock(emu);
        } else {
            mmu->io_registers[io_reg_addr] = data & 0x83;
        }
        break;
    case DIV_LSB:
    case DIV:
        // writing to DIV resets it to 0
        mmu->io_registers[DIV_LSB - IO] = 0;
        mmu->io_registers[DIV - IO] = 0;
        mmu->io_registers[TIMA - IO] = mmu->io_registers[TMA - IO];
        break;
    case TIMA:
        if (emu->timer->tima_loading_value == -1)
            mmu->io_registers[io_reg_addr] = mmu->io_registers[TMA - IO];
        else
            mmu->io_registers[io_reg_addr] = data;
        break;
    case TMA:
        if (emu->timer->tima_loading_value == -1)
            mmu->io_registers[TIMA - IO] = data;
        emu->timer->old_tma = mmu->io_registers[io_reg_addr];
        mmu->io_registers[io_reg_addr] = data;
        break;
    case TAC:
        mmu->io_registers[io_reg_addr] = data;
        switch (data & 0x03) {
        case 0x00: emu->timer->tima_increase_div_bit = 9; break;
        case 0x01: emu->timer->tima_increase_div_bit = 3; break;
        case 0x02: emu->timer->tima_increase_div_bit = 5; break;
        case 0x03: emu->timer->tima_increase_div_bit = 7; break;
        }
        break;
    case NR10:
        if (!IS_APU_ENABLED(emu))
            return;
        mmu->io_registers[io_reg_addr] = data;
        break;
    case NR11:
        if (!IS_APU_ENABLED(emu))
            return;
        mmu->io_registers[io_reg_addr] = data;
        emu->apu->channel1.length_counter = 64 - (data & 0x3F);
        break;
    case NR12:
        if (!IS_APU_ENABLED(emu))
            return;
        mmu->io_registers[io_reg_addr] = data;
        if (!(data >> 3)) // if dac disabled
            APU_DISABLE_CHANNEL(emu, APU_CHANNEL_1);
        break;
    case NR13:
        if (!IS_APU_ENABLED(emu))
            return;
        mmu->io_registers[io_reg_addr] = data;
        break;
    case NR14:
        if (!IS_APU_ENABLED(emu))
            return;
        mmu->io_registers[io_reg_addr] = data;
        if (CHECK_BIT(data, 7) && (data >> 3)) // if trigger requested and dac enabled
            apu_channel_trigger(emu, &emu->apu->channel1);
        break;
    case NR20:
        if (!IS_APU_ENABLED(emu))
            return;
        mmu->io_registers[io_reg_addr] = data;
        break;
    case NR21:
        if (!IS_APU_ENABLED(emu))
            return;
        mmu->io_registers[io_reg_addr] = data;
        emu->apu->channel2.length_counter = 64 - (data & 0x3F);
        break;
    case NR22:
        if (!IS_APU_ENABLED(emu))
            return;
        mmu->io_registers[io_reg_addr] = data;
        if (!(data >> 3)) // if dac disabled
            APU_DISABLE_CHANNEL(emu, APU_CHANNEL_2);
        break;
    case NR23:
        if (!IS_APU_ENABLED(emu))
            return;
        mmu->io_registers[io_reg_addr] = data;
        break;
    case NR24:
        if (!IS_APU_ENABLED(emu))
            return;
        mmu->io_registers[io_reg_addr] = data;
        if (CHECK_BIT(data, 7) && (data >> 3)) // if trigger requested and dac enabled
            apu_channel_trigger(emu, &emu->apu->channel2);
        break;
    case NR30:
        if (!IS_APU_ENABLED(emu))
            return;
        mmu->io_registers[io_reg_addr] = data;
        if (!(data >> 7)) // if dac disabled
            APU_DISABLE_CHANNEL(emu, APU_CHANNEL_3);
        break;
    case NR31:
        if (!IS_APU_ENABLED(emu))
            return;
        mmu->io_registers[io_reg_addr] = data;
        emu->apu->channel3.length_counter = 256 - data;
        break;
    case NR32:
        if (!IS_APU_ENABLED(emu))
            return;
        mmu->io_registers[io_reg_addr] = data;
        break;
    case NR33:
        if (!IS_APU_ENABLED(emu))
            return;
        mmu->io_registers[io_reg_addr] = data;
        break;
    case NR34:
        if (!IS_APU_ENABLED(emu))
            return;
        mmu->io_registers[io_reg_addr] = data;
        if (CHECK_BIT(data, 7) && (data >> 7)) // if trigger requested and dac enabled
            apu_channel_trigger(emu, &emu->apu->channel3);
        break;
    case NR40:
        if (!IS_APU_ENABLED(emu))
            return;
        mmu->io_registers[io_reg_addr] = data;
        break;
    case NR41:
        if (!IS_APU_ENABLED(emu))
            return;
        mmu->io_registers[io_reg_addr] = data;
        emu->apu->channel4.length_counter = 64 - (data & 0x3F);
        break;
    case NR42:
        if (!IS_APU_ENABLED(emu))
            return;
        mmu->io_registers[io_reg_addr] = data;
        if (!(data >> 3)) // if dac disabled
            APU_DISABLE_CHANNEL(emu, APU_CHANNEL_4);
        break;
    case NR43:
        if (!IS_APU_ENABLED(emu))
            return;
        mmu->io_registers[io_reg_addr] = data;
        break;
    case NR44:
        if (!IS_APU_ENABLED(emu))
            return;
        mmu->io_registers[io_reg_addr] = data;
        if (CHECK_BIT(data, 7) && (data >> 3)) // if trigger requested and dac enabled
            apu_channel_trigger(emu, &emu->apu->channel4);
        break;
    case NR50:
        if (!IS_APU_ENABLED(emu))
            return;
        mmu->io_registers[io_reg_addr] = data;
        break;
    case NR51:
        if (!IS_APU_ENABLED(emu))
            return;
        mmu->io_registers[io_reg_addr] = data;
        break;
    case NR52:
        CHANGE_BIT(mmu->io_registers[NR52 - IO], 7, data >> 7);
        if (!IS_APU_ENABLED(emu))
            memset(&mmu->io_registers[NR10 - IO], 0x00, 32 * sizeof(byte_t)); // clear all registers
        break;
    case WAVE_RAM ... LCDC - 1:
        if (!CHECK_BIT(mmu->io_registers[NR30 - IO], 7))
            mmu->io_registers[io_reg_addr] = data;
        break;
    case STAT:
        mmu->io_registers[io_reg_addr] = 0x80 | (data & 0x78) | (mmu->io_registers[io_reg_addr] & 0x07);
        break;
    case LY: break; // read only
    case LYC:
        // a write to LYC triggers an immediate LY=LYC comparison
        mmu->io_registers[io_reg_addr] = data;
        ppu_ly_lyc_compare(emu);
        break;
    case DMA:
        // writing to this register starts an OAM DMA transfer
        mmu->io_registers[io_reg_addr] = data;
        for (unsigned int i = 0; i < sizeof(mmu->oam_dma.starting_statuses); i++) {
            if (mmu->oam_dma.starting_statuses[i] == OAM_DMA_NO_INIT) {
                mmu->oam_dma.starting_statuses[i] = OAM_DMA_INIT_BEGIN;
                mmu->oam_dma.starting_count++;
                break;
            }
        }
        break;
    case KEY1:
        mmu->io_registers[io_reg_addr] |= data & 0x01;
        break;
    case VBK:
        if (emu->mode == CGB) {
            mmu->io_registers[io_reg_addr] = data & 0x01;
            mmu->vram_bank_addr_offset = (GBC_CURRENT_VRAM_BANK(mmu) * VRAM_BANK_SIZE) - VRAM;
        }
        break;
    case BANK:
        // disable boot rom
        if ((emu->mode == DMG && data == 0x01) || (emu->mode == CGB && data == 0x11))
            mmu->boot_finished = 1;
        mmu->io_registers[io_reg_addr] = data;
        break;
    case HDMA5:
        if (emu->mode == DMG)
            break;

        // writing to this register starts a VRAM DMA transfer

        if (mmu->hdma.progress > 0 && mmu->hdma.type == HDMA && !GET_BIT(data, 7)) {
            // cancel HDMA
            mmu->hdma.progress = 0;
            mmu->io_registers[io_reg_addr] = 0x80 | data;
            return;
        }
        // if mmu->hdma.progress > 0 && mmu->hdma.type == HDMA && GET_BIT(data, 7)
        // --> let the execution continue to restart HDMA

        // bit 7 of HDMA5 is always 0, bits 0-6 are the size of the DMA
        mmu->io_registers[io_reg_addr] = data & 0x7F;
        mmu->hdma.progress = mmu->io_registers[io_reg_addr] + 1;

        // bit 7 of data is 0 if requesting GDMA, 1 if requesting HDMA
        mmu->hdma.type = GET_BIT(data, 7);
        if (mmu->hdma.type == GDMA) {
            mmu->hdma.lock_cpu = 1;
        } else { // HDMA
            // Start HDMA now if we are in ppu HBLANK mode or if LCD is disabled.
            // In the latter case, only the first 0x10 bytes block are copied, the rest are done at HBLANK once LCD turns on.
            mmu->hdma.allow_hdma_block = PPU_IS_MODE(emu, PPU_MODE_HBLANK) || !IS_LCD_ENABLED(emu);
        }

        mmu->hdma.src_address = ((mmu->io_registers[HDMA1 - IO] << 8) | mmu->io_registers[HDMA2 - IO]) & 0xFFF0;
        mmu->hdma.dest_address = 0x8000 | (((mmu->io_registers[HDMA3 - IO] << 8) | mmu->io_registers[HDMA4 - IO]) & 0x1FF0);

        mmu->hdma.initializing = 1;

        // if (mmu->hdma.type) // HBLANK DMA (HDMA)
        //     printf("HDMA size=%d (%d blocs), vram bank=%d, wram bank n=%d, src=%x, dest=%x\n", (mmu->io_registers[io_reg_addr] + 1) * 0x10, mmu->io_registers[io_reg_addr] + 1, GBC_CURRENT_VRAM_BANK(mmu), GBC_CURRENT_WRAM_BANK(mmu), mmu->hdma.src_address, mmu->hdma.dest_address);
        // else // General purpose DMA (GDMA)
        //     printf("GDMA size=%d (%d blocs), vram bank=%d, wram bank n=%d, src=%x, dest=%x\n", (mmu->io_registers[io_reg_addr] + 1) * 0x10, mmu->io_registers[io_reg_addr] + 1, GBC_CURRENT_VRAM_BANK(mmu), GBC_CURRENT_WRAM_BANK(mmu), mmu->hdma.src_address, mmu->hdma.dest_address);
        break;
    case BGPD:
        if (emu->mode == DMG)
            break;

        if (!PPU_IS_MODE(emu, PPU_MODE_DRAWING)) {
            byte_t cram_address = mmu->io_registers[BGPI - IO] & 0x3F;
            mmu->cram_bg[cram_address] = data;
            // printf("write %d in cram_bg %d\n", data, cram_address);
        }

        // increment BGPI address if auto increment (bit.7) of BGPI is set
        if (CHECK_BIT(mmu->io_registers[BGPI - IO], 7)) {
            byte_t new_bgpi_address = (mmu->io_registers[BGPI - IO] & 0x3F) + 1;
            if (new_bgpi_address > 0x3F)
                new_bgpi_address = 0;
            mmu->io_registers[BGPI - IO] = new_bgpi_address;
            SET_BIT(mmu->io_registers[BGPI - IO], 7);
        }
        break;
    case OBPD:
        if (emu->mode == DMG)
            break;

        if (!PPU_IS_MODE(emu, PPU_MODE_DRAWING)) {
            byte_t cram_address = mmu->io_registers[OBPI - IO] & 0x3F;
            mmu->cram_obj[cram_address] = data;
            // printf("write %d in cram_obj %d\n", data, cram_address);
        }

        // increment OBPI address if auto increment (bit.7) of OBPI is set
        if (CHECK_BIT(mmu->io_registers[OBPI - IO], 7)) {
            byte_t new_obpi_address = (mmu->io_registers[OBPI - IO] & 0x3F) + 1;
            if (new_obpi_address > 0x3F)
                new_obpi_address = 0;
            mmu->io_registers[OBPI - IO] = new_obpi_address;
            SET_BIT(mmu->io_registers[OBPI - IO], 7);
        }
        break;
    case SVBK:
        if (emu->mode == CGB) {
            mmu->io_registers[io_reg_addr] = data & 0x07;
            mmu->wram_bankn_addr_offset = ((GBC_CURRENT_WRAM_BANK(mmu) - 1) * WRAM_BANK_SIZE) - WRAM_BANK0;
        }
        break;
    case 0xFF74:
        if (emu->mode == CGB)
            mmu->io_registers[io_reg_addr] = data; // only writable in CGB mode
        break;
    case 0xFF75:
        mmu->io_registers[io_reg_addr] = emu->mode == CGB ? data & 0x70 : data;
        break;
    case 0xFF76:
        if (emu->mode == DMG)
            mmu->io_registers[io_reg_addr] = data; // only writable in DMG mode
        break;
    case 0xFF77:
        if (emu->mode == DMG)
            mmu->io_registers[io_reg_addr] = data; // only writable in DMG mode
        break;
    default:
        mmu->io_registers[io_reg_addr] = data;
        break;
    }
}

byte_t mmu_read_io_src(emulator_t *emu, word_t address, io_source_t io_src) {
    mmu_t *mmu = emu->mmu;

    switch (address & 0xF000) {
    case ROM_BANK0:
        if (!mmu->boot_finished) {
            if (emu->mode == DMG && address < 0x100)
                return dmg_boot[address];
            if (emu->mode == CGB && (address < 0x100 || (address >= 0x200 && address < sizeof(cgb_boot))))
                return cgb_boot[address];
        }
        // fallthrough
    case ROM_BANK0 + 0x1000:
    case ROM_BANK0 + 0x2000:
    case ROM_BANK0 + 0x3000:
        return mmu->rom[mmu->rom_bank0_addr + address];
    case ROM_BANKN:
    case ROM_BANKN + 0x1000:
    case ROM_BANKN + 0x2000:
    case ROM_BANKN + 0x3000:
        return mmu->rom[mmu->rom_bankn_addr + address];
    case VRAM:
    case VRAM + 0x1000:
        if (io_src == IO_SRC_GDMA_HDMA)
            return 0xFF; // src can be inside vram but it reads incorrect data: see TCAGBD.pdf for more details
        if (io_src == IO_SRC_CPU && IS_LCD_ENABLED(emu) && PPU_IS_MODE(emu, PPU_MODE_DRAWING))
            return 0xFF; // VRAM inaccessible by cpu while ppu in mode 3 and LCD is enabled (return undefined data)
        return mmu->vram[mmu->vram_bank_addr_offset + address];
    case ERAM:
    case ERAM + 0x1000:
        return mbc_read_eram(emu, address);
    case WRAM_BANK0:
        return mmu->wram[address - WRAM_BANK0];
    case WRAM_BANKN:
        return mmu->wram[mmu->wram_bankn_addr_offset + address];
    case ECHO:
        return mmu->wram[address - ECHO];
    case 0xF000:
        // From CasualPokePlayer (https://github.com/skylersaleh/SkyEmu/blob/52a08105c06f1a4f1e9215b2f9ab83fe04ee6236/src/gb.h#L1267):
        // in most cases echo ram is only E000-FDFF. 
        // oam dma is one of the exceptions here which have the entire E000-FFFF
        // region as echo ram for dma source (therefore only for memory reads by the oam dma)
        if (address < OAM || io_src == IO_SRC_OAM_DMA || io_src == IO_SRC_GDMA_HDMA) // we are still in ECHO ram
            return mmu->wram[(mmu->wram_bankn_addr_offset - (ECHO - WRAM_BANK0)) + address];

        if (address < UNUSABLE) { // we are in OAM
            // OAM inaccessible by cpu while ppu in mode 2 or 3 and LCD is enabled (return undefined data)
            if (io_src == IO_SRC_CPU && IS_LCD_ENABLED(emu) && ((PPU_IS_MODE(emu, PPU_MODE_OAM) || PPU_IS_MODE(emu, PPU_MODE_DRAWING))))
                return 0xFF;

            // contrary to most sources, an OAM DMA transfer doesn't prevent the CPU to access all memory except HRAM: it only prevent access to the OAM memory region
            // but it has some quirks for the other memory regions (check links below):
            // https://www.reddit.com/r/EmuDev/comments/5hahss/comment/daz9cbi/?utm_source=share&utm_medium=web3x&utm_name=web3xcss&utm_term=1&utm_content=share_button
            // https://github.com/Gekkio/mooneye-gb/issues/39#issuecomment-265953981
            if (io_src == IO_SRC_CPU && IS_OAM_DMA_RUNNING(mmu))
                return 0xFF;

            return mmu->oam[address - OAM];
        }

        if (address < IO) // we are in UNUSABLE
            return 0xFF;

        if (address < HRAM)
            return read_io_register(emu, address);

        if (address < IE)
            return mmu->hram[address - HRAM];

        return mmu->ie;
    default:
        eprintf("invalid cpu read at address 0x%X\n", address);
        exit(EXIT_FAILURE);
    }
}

void mmu_write_io_src(emulator_t *emu, word_t address, byte_t data, io_source_t io_src) {
    mmu_t *mmu = emu->mmu;

    switch (address & 0xF000) {
    case ROM_BANK0:
    case ROM_BANK0 + 0x1000:
    case ROM_BANK0 + 0x2000:
    case ROM_BANK0 + 0x3000:
    case ROM_BANKN:
    case ROM_BANKN + 0x1000:
    case ROM_BANKN + 0x2000:
    case ROM_BANKN + 0x3000:
        mbc_write_registers(emu, address, data);
        break;
    case VRAM:
    case VRAM + 0x1000:
        // VRAM inaccessible by cpu while ppu in mode 3 and LCD is enabled (return undefined data)
        if (io_src == IO_SRC_CPU && IS_LCD_ENABLED(emu) && PPU_IS_MODE(emu, PPU_MODE_DRAWING))
            return;
        mmu->vram[mmu->vram_bank_addr_offset + address] = data;
        break;
    case ERAM:
    case ERAM + 0x1000:
        mbc_write_eram(emu, address, data);
        break;
    case WRAM_BANK0:
        mmu->wram[address - WRAM_BANK0] = data;
        break;
    case WRAM_BANKN:
        mmu->wram[mmu->wram_bankn_addr_offset + address] = data;
        break;
    case ECHO:
        mmu->wram[address - ECHO] = data;
        break;
    case 0xF000:
        if (address < OAM) { // we are still in echo ram
            mmu->wram[(mmu->wram_bankn_addr_offset - (ECHO - WRAM_BANK0)) + address] = data;
        } else if (address < UNUSABLE) {
            // OAM inaccessible by cpu while ppu in mode 2 or 3 and LCD is enabled (return undefined data)
            if (io_src == IO_SRC_CPU && IS_LCD_ENABLED(emu) && ((PPU_IS_MODE(emu, PPU_MODE_OAM) || PPU_IS_MODE(emu, PPU_MODE_DRAWING))))
                break;

            // contrary to most sources, an OAM DMA transfer doesn't prevent the CPU to access all memory except HRAM: it only prevent access to the OAM memory region
            // but it has some quirks for the other memory regions (check links below):
            // https://www.reddit.com/r/EmuDev/comments/5hahss/comment/daz9cbi/?utm_source=share&utm_medium=web3x&utm_name=web3xcss&utm_term=1&utm_content=share_button
            // https://github.com/Gekkio/mooneye-gb/issues/39#issuecomment-265953981
            if (io_src == IO_SRC_CPU && IS_OAM_DMA_RUNNING(mmu))
                break;

            mmu->oam[address - OAM] = data;
        } else if (address < IO) {
            // UNUSABLE memory is unusable
        } else if (address < HRAM) {
            write_io_register(emu, address, data);
        } else if (address < IE) {
            mmu->hram[address - HRAM] = data;
        } else {
            mmu->ie = data;
        }
        break;
    default:
        eprintf("invalid write of 0x%02X at address 0x%X\n", data, address);
        exit(EXIT_FAILURE);
    }
}

// serialize everything except rom
#define SERIALIZED_MEMBERS                                            \
    X(rom_size)                                                       \
    Y(vram, emu->mode == CGB, 2 * VRAM_BANK_SIZE, VRAM_BANK_SIZE)     \
    Z(eram, eram_banks, ERAM_BANK_SIZE)                               \
    Y(wram, emu->mode == CGB, 8 * WRAM_BANK_SIZE, 2 * WRAM_BANK_SIZE) \
    X(oam)                                                            \
    X(io_registers)                                                   \
    X(hram)                                                           \
    X(ie)                                                             \
    W(cram_bg)                                                        \
    W(cram_obj)                                                       \
    X(boot_finished)                                                  \
    X(hdma.initializing)                                              \
    X(hdma.allow_hdma_block)                                          \
    X(hdma.lock_cpu)                                                  \
    X(hdma.type)                                                      \
    X(hdma.progress)                                                  \
    X(hdma.src_address)                                               \
    X(hdma.dest_address)                                              \
    X(oam_dma.starting_statuses)                                      \
    X(oam_dma.starting_count)                                         \
    X(oam_dma.progress)                                               \
    X(oam_dma.src_address)                                            \
    X(vram_bank_addr_offset)                                          \
    X(wram_bankn_addr_offset)                                         \
    X(rom_banks)                                                      \
    X(eram_banks)                                                     \
    X(rom_bank0_addr)                                                 \
    X(rom_bankn_addr)                                                 \
    X(eram_bank_addr)                                                 \
    X(has_battery)                                                    \
    X(has_rumble)                                                     \
    X(has_rtc)

#define X(value) SERIALIZED_LENGTH(value);
#define Y(...) SERIALIZED_LENGTH_COND_LITERAL(__VA_ARGS__);
#define Z(...) SERIALIZED_LENGTH_FROM_MEMBER(__VA_ARGS__);
#define W(...) SERIALIZED_LENGTH_IF_CGB(__VA_ARGS__);
SERIALIZED_SIZE_FUNCTION(mmu_t, mmu,
    SERIALIZED_MEMBERS
    {
        mbc_t *tmp = &emu->mmu->mbc;
        MBC_SERIALIZED_MEMBERS
    }
)
#undef W
#undef Z
#undef Y
#undef X

#define X(value) SERIALIZE(value);
#define Y(...) SERIALIZE_COND_LITERAL(__VA_ARGS__);
#define Z(...) SERIALIZE_FROM_MEMBER(__VA_ARGS__);
#define W(...) SERIALIZE_IF_CGB(__VA_ARGS__);
SERIALIZER_FUNCTION(mmu_t, mmu,
    SERIALIZED_MEMBERS
    {
        mbc_t *tmp = &emu->mmu->mbc;
        MBC_SERIALIZED_MEMBERS
    }
)
#undef W
#undef Z
#undef Y
#undef X

#define X(value) UNSERIALIZE(value);
#define Y(...) UNSERIALIZE_COND_LITERAL(__VA_ARGS__);
#define Z(...) UNSERIALIZE_FROM_MEMBER(__VA_ARGS__);
#define W(...) UNSERIALIZE_IF_CGB(__VA_ARGS__);
UNSERIALIZER_FUNCTION(mmu_t, mmu,
    SERIALIZED_MEMBERS
    {
        mbc_t *tmp = &emu->mmu->mbc;
        MBC_SERIALIZED_MEMBERS
    }
)
#undef W
#undef Z
#undef Y
#undef X
