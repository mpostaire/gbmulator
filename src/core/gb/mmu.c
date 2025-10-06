#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "gb_priv.h"
#include "boot.h"
#include "mbc.h"

#define GBC_CURRENT_VRAM_BANK(mmu) ((mmu)->io_registers[IO_VBK] & 0x01)
#define GBC_CURRENT_WRAM_BANK(mmu) (((mmu)->io_registers[IO_SVBK] & 0x07) == 0 ? 1 : ((mmu)->io_registers[IO_SVBK] & 0x07))

int parse_header_mbc_byte(uint8_t mbc_byte, uint8_t *mbc_type, uint8_t *has_eram, uint8_t *has_battery, uint8_t *has_rtc, uint8_t *has_rumble) {
    uint8_t tmp_mbc_type = 0, tmp_has_eram = 0, tmp_has_battery = 0, tmp_has_rtc = 0, tmp_has_rumble = 0;

    switch (mbc_byte) {
    case 0x00:
        tmp_mbc_type = ROM_ONLY;
        break;
    case 0x01:
        tmp_mbc_type = MBC1;
        break;
    case 0x02:
        tmp_mbc_type = MBC1;
        tmp_has_eram = 1;
        break;
    case 0x03:
        tmp_mbc_type = MBC1;
        tmp_has_eram = 1;
        tmp_has_battery = 1;
        break;
    case 0x05:
        tmp_mbc_type = MBC2;
        break;
    case 0x06:
        tmp_mbc_type = MBC2;
        tmp_has_eram = 1;
        tmp_has_battery = 1;
        break;
    case 0x0F:
        tmp_mbc_type = MBC3;
        tmp_has_rtc = 1;
        tmp_has_battery = 1;
        break;
    case 0x10:
        tmp_mbc_type = MBC3;
        tmp_has_rtc = 1;
        tmp_has_battery = 1;
        tmp_has_eram = 1;
        break;
    case 0x11:
        tmp_mbc_type = MBC3;
        break;
    case 0x12:
        tmp_mbc_type = MBC3;
        tmp_has_eram = 1;
        break;
    case 0x13:
        tmp_mbc_type = MBC3;
        tmp_has_battery = 1;
        tmp_has_eram = 1;
        break;
    case 0x19:
        tmp_mbc_type = MBC5;
        break;
    case 0x1A:
        tmp_mbc_type = MBC5;
        tmp_has_eram = 1;
        break;
    case 0x1B:
        tmp_mbc_type = MBC5;
        tmp_has_eram = 1;
        tmp_has_battery = 1;
        break;
    case 0x1C:
        tmp_mbc_type = MBC5;
        tmp_has_rumble = 1;
        break;
    case 0x1D:
        tmp_mbc_type = MBC5;
        tmp_has_rumble = 1;
        tmp_has_eram = 1;
        break;
    case 0x1E:
        tmp_mbc_type = MBC5;
        tmp_has_rumble = 1;
        tmp_has_eram = 1;
        tmp_has_battery = 1;
        break;
    // case 0x20:
    //     tmp_mbc_type = MBC6;
    //     break;
    case 0x22:
        tmp_mbc_type = MBC7;
        break;
    case 0xFC:
        tmp_mbc_type = CAMERA;
        break;
    // case 0xFD:
    //     tmp_mbc_type = TAMA5;
    //     break;
    // case 0xFE:
    //     tmp_mbc_type = HuC3;
    //     tmp_has_rtc = 1;
    //     tmp_has_eram = 1;
    //     tmp_has_battery = 1;
    //     break;
    case 0xFF:
        tmp_mbc_type = HuC1;
        tmp_has_eram = 1;
        tmp_has_battery = 1;
        break;
    default: return 0;
    }

    if (mbc_type) *mbc_type = tmp_mbc_type;
    if (has_eram) *has_eram = tmp_has_eram;
    if (has_battery) *has_battery = tmp_has_battery;
    if (has_rtc) *has_rtc = tmp_has_rtc;
    if (has_rumble) *has_rumble = tmp_has_rumble;

    return 1;
}

int validate_header_checksum(const uint8_t *rom) {
    uint8_t checksum = 0;
    for (int i = 0x0134; i <= 0x014C; i++)
        checksum = checksum - rom[i] - 1;
    return checksum == rom[0x014D];
}

static int parse_cartridge(gb_t *gb) {
    gb_mmu_t *mmu = gb->mmu;

    // 8-bit cartridge header checksum validation
    if (!validate_header_checksum(mmu->rom)) {
        eprintf("invalid checksum\n");
        return 0;
    }

    uint8_t has_eram = 0;
    if (!parse_header_mbc_byte(mmu->rom[0x0147], &mmu->mbc.type, &has_eram, &mmu->has_battery, &mmu->has_rtc, &mmu->has_rumble)) {
        eprintf("MBC byte %02X not supported\n", mmu->rom[0x0147]);
        return 0;
    }

    // detect MBC1M
    if (mmu->mbc.type == MBC1 && mmu->rom_size == 0x100000) {
        const unsigned int addrs[] = { 0x00104, 0x40104, 0x80104, 0xC0104 };
        uint8_t logo[] = {
            0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
            0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E, 0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
            0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E
        };

        uint8_t matches = 0;
        for (uint8_t i = 0; i < 4; i++) {
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
    memcpy(gb->rom_title, (char *) &mmu->rom[0x0134], 16);
    gb->rom_title[16] = '\0';
    uint8_t cgb_flag = mmu->rom[0x0143] & 0xC0;
    if (cgb_flag)
        gb->rom_title[15] = '\0';

    // remove trailing non alphanumeric characters from the rom title
    for (char *c = &gb->rom_title[16]; c >= gb->rom_title && !isalnum(*c); c--)
        *c = '\0';

    return 1;
}

int mmu_init(gb_t *gb, const uint8_t *rom, size_t rom_size) {
    gb_mmu_t *mmu = xcalloc(1, sizeof(*mmu));
    mmu->rom = xcalloc(1, rom_size);
    mmu->rom_size = rom_size;
    memcpy(mmu->rom, rom, mmu->rom_size);
    gb->mmu = mmu;

    if (!parse_cartridge(gb)) {
        mmu_quit(gb);
        return 0;
    }

    mmu->mbc.mbc1.bank_lo = 1;
    // mmu->rom_bank0_addr = 0; // initialized to 0 by xcalloc
    // mmu->rom_bankn_addr = 0; // initialized to MMU_ROM_BANKN - ROM_BANK_SIZE = 0 by xcalloc
    // mmu->eram_bank_addr = 0; // initialized to 0 by xcalloc
    mmu->wram_bankn_addr_offset = -MMU_WRAM_BANK0;
    mmu->vram_bank_addr_offset = -MMU_VRAM;

    // mmu->mbc7.accelerometer.latched_x = 0x8000;
    // mmu->mbc7.accelerometer.latched_y = 0x8000;
    // mmu->mbc7.accelerometer.latch_ready = 1;

    if (mmu->mbc.type == CAMERA) {
        mmu->mbc.camera.sensor_image = xcalloc(GB_CAMERA_SENSOR_HEIGHT * GB_CAMERA_SENSOR_WIDTH, sizeof(*mmu->mbc.camera.sensor_image));
    } else if (mmu->mbc.type == MBC7) {
        mmu->mbc.mbc7.accelerometer.latched_x = 0x81D0;
        mmu->mbc.mbc7.accelerometer.latched_y = 0x81D0;
    }

    mmu->dmg_boot_rom = dmg_boot;
    mmu->cgb_boot_rom = cgb_boot;

    return 1;
}

void mmu_quit(gb_t *gb) {
    free(gb->mmu->rom);
    if (gb->mmu->mbc.type == CAMERA)
        free(gb->mmu->mbc.camera.sensor_image);
    free(gb->mmu);
}

static inline uint8_t is_oam_locked_for_cpu_read(gb_t *gb) {
    // contrary to most sources, an OAM DMA transfer doesn't prevent the CPU to access all memory except HRAM: it only prevent access to the OAM memory region
    // but it has some quirks for the other memory regions (check links below):
    // https://www.reddit.com/r/EmuDev/comments/5hahss/comment/daz9cbi/?utm_source=share&utm_medium=web3x&utm_name=web3xcss&utm_term=1&utm_content=share_button
    // https://github.com/Gekkio/mooneye-gb/issues/39#issuecomment-265953981
    if (IS_OAM_DMA_RUNNING(gb->mmu))
        return 1;

    // OAM inaccessible by cpu while ppu in mode 2 or 3 and LCD is enabled (return undefined data)
    return IS_LCD_ENABLED(gb) && (PPU_STAT_IS_MODE(gb, PPU_MODE_OAM) || PPU_STAT_IS_MODE(gb, PPU_MODE_DRAWING) || gb->ppu->pending_stat_mode == PPU_MODE_OAM);
}

static inline uint8_t is_oam_locked_for_cpu_write(gb_t *gb) {
    // contrary to most sources, an OAM DMA transfer doesn't prevent the CPU to access all memory except HRAM: it only prevent access to the OAM memory region
    // but it has some quirks for the other memory regions (check links below):
    // https://www.reddit.com/r/EmuDev/comments/5hahss/comment/daz9cbi/?utm_source=share&utm_medium=web3x&utm_name=web3xcss&utm_term=1&utm_content=share_button
    // https://github.com/Gekkio/mooneye-gb/issues/39#issuecomment-265953981
    if (IS_OAM_DMA_RUNNING(gb->mmu))
        return 1;

    if (!IS_LCD_ENABLED(gb))
        return 0;

    uint8_t lcd_booting_up_into_drawing = PPU_STAT_IS_MODE(gb, PPU_MODE_HBLANK) && gb->ppu->pending_stat_mode == PPU_MODE_DRAWING;
    uint8_t starting_drawing = gb->ppu->mode == PPU_MODE_DRAWING && PPU_STAT_IS_MODE(gb, PPU_MODE_OAM);
    if (starting_drawing || lcd_booting_up_into_drawing)
        return 0;

    return PPU_STAT_IS_MODE(gb, PPU_MODE_DRAWING) || PPU_STAT_IS_MODE(gb, PPU_MODE_OAM);
}

static inline uint8_t is_vram_locked_for_cpu_read(gb_t *gb) {
    // VRAM inaccessible by cpu while ppu in mode 3 and LCD is enabled (return undefined data)
    return IS_LCD_ENABLED(gb) && (PPU_STAT_IS_MODE(gb, PPU_MODE_DRAWING) || (PPU_STAT_IS_MODE(gb, PPU_MODE_OAM) && gb->ppu->pending_stat_mode == PPU_MODE_DRAWING));
}

static inline uint8_t is_vram_locked_for_cpu_write(gb_t *gb) {
    // VRAM inaccessible by cpu while ppu in mode 3 and LCD is enabled (return undefined data)
    return IS_LCD_ENABLED(gb) && PPU_STAT_IS_MODE(gb, PPU_MODE_DRAWING);
}

static inline uint8_t read_io_register(gb_t *gb, uint8_t io_reg_addr) {
    gb_mmu_t *mmu = gb->mmu;

    switch (io_reg_addr) {
    case IO_P1:
        // Reading from P1 register returns joypad input state according to its current bit 4 or 5 value
        return joypad_get_input(gb);
    case IO_SB: return mmu->io_registers[io_reg_addr];
    case IO_SC: return mmu->io_registers[io_reg_addr] | (gb->cgb_mode_enabled ? 0x7C : 0x7E);
    case 0x03: return 0xFF;
    case IO_DIV: return gb->timer->div_timer >> 8;
    case IO_TIMA: return mmu->io_registers[io_reg_addr];
    case IO_TMA: return mmu->io_registers[io_reg_addr];
    case IO_TAC: return mmu->io_registers[io_reg_addr] | 0xF8;
    case 0x08 ... IO_IF - 1: return 0xFF;
    case IO_IF: return mmu->io_registers[io_reg_addr] | 0xE0;
    case IO_NR10: return mmu->io_registers[io_reg_addr] | 0x80;
    case IO_NR11: return mmu->io_registers[io_reg_addr] | 0x3F;
    case IO_NR12: return mmu->io_registers[io_reg_addr];
    case IO_NR13: return 0xFF;
    case IO_NR14: return mmu->io_registers[io_reg_addr] | 0xBF;
    case IO_NR20: return 0xFF;
    case IO_NR21: return mmu->io_registers[io_reg_addr] | 0x3F;
    case IO_NR22: return mmu->io_registers[io_reg_addr];
    case IO_NR23: return 0xFF;
    case IO_NR24: return mmu->io_registers[io_reg_addr] | 0xBF;
    case IO_NR30: return mmu->io_registers[io_reg_addr] | 0x7F;
    case IO_NR31: return 0xFF;
    case IO_NR32: return mmu->io_registers[io_reg_addr] | 0x9F;
    case IO_NR33: return 0xFF;
    case IO_NR34: return mmu->io_registers[io_reg_addr] | 0xBF;
    case IO_NR40: return 0xFF;
    case IO_NR41: return 0xFF;
    case IO_NR42: return mmu->io_registers[io_reg_addr];
    case IO_NR43: return mmu->io_registers[io_reg_addr];
    case IO_NR44: return mmu->io_registers[io_reg_addr] | 0xBF;
    case IO_NR50: return mmu->io_registers[io_reg_addr];
    case IO_NR51: return mmu->io_registers[io_reg_addr];
    case IO_NR52: return mmu->io_registers[io_reg_addr] | 0x70;
    case 0x27 ... IO_WAVE_RAM - 1: return 0xFF;
    case IO_WAVE_RAM ... IO_LCDC - 1: return mmu->io_registers[io_reg_addr];
    case IO_LCDC: return mmu->io_registers[io_reg_addr];
    case IO_STAT: return mmu->io_registers[io_reg_addr] | 0x80;
    case IO_SCY: return mmu->io_registers[io_reg_addr];
    case IO_SCX: return mmu->io_registers[io_reg_addr];
    case IO_LY: return mmu->io_registers[io_reg_addr];
    case IO_LYC: return mmu->io_registers[io_reg_addr];
    case IO_DMA: return mmu->io_registers[io_reg_addr];
    case IO_BGP: return mmu->io_registers[io_reg_addr];
    case IO_OBP0: return mmu->io_registers[io_reg_addr];
    case IO_OBP1: return mmu->io_registers[io_reg_addr];
    case IO_WY: return mmu->io_registers[io_reg_addr];
    case IO_WX: return mmu->io_registers[io_reg_addr];
    case IO_KEY0: return 0xFF;
    case IO_KEY1: return gb->cgb_mode_enabled ? (mmu->io_registers[io_reg_addr] | 0x7E) : 0xFF;
    case 0x4E: return 0xFF;
    case IO_VBK: return gb->base->opts.mode == GBMULATOR_MODE_GBC ? 0xFE | (mmu->io_registers[io_reg_addr] & 0x01) : 0xFF;
    case IO_BANK: return 0xFF;
    case IO_HDMA1 ... IO_HDMA4: return 0xFF;
    case IO_HDMA5: return gb->cgb_mode_enabled ? mmu->io_registers[io_reg_addr] : 0xFF;
    case IO_RP:
        if (gb->cgb_mode_enabled) {
            if (!gb->base->ir.other_device)
                return mmu->io_registers[io_reg_addr] | 0x3E;

            uint8_t other_gb_led = ((gb_t *) gb->base->ir.other_device->impl)->mmu->io_registers[IO_RP] & 0x01;
            uint8_t read_bit = (mmu->io_registers[io_reg_addr] & 0xC0) == 0xC0 ? !other_gb_led : 0x01;
            CHANGE_BIT(mmu->io_registers[io_reg_addr], 1, read_bit);
            return mmu->io_registers[io_reg_addr] | 0x3C;
        }
        return 0xFF;
    case 0x57 ... 0x67: return 0xFF;
    case IO_BGPI: return gb->base->opts.mode == GBMULATOR_MODE_GBC ? mmu->io_registers[io_reg_addr] | 0x40 : 0xFF;
    case IO_BGPD: {
        if (gb->base->opts.mode != GBMULATOR_MODE_GBC || gb->ppu->mode == PPU_MODE_DRAWING)
            return 0xFF;

        uint8_t cram_address = mmu->io_registers[IO_BGPI] & 0x3F;
        return mmu->cram_bg[cram_address];
    }
    case IO_OBPI: return gb->base->opts.mode == GBMULATOR_MODE_GBC ? mmu->io_registers[io_reg_addr] | 0x40 : 0xFF;
    case IO_OBPD: {
        if (!gb->cgb_mode_enabled || gb->ppu->mode == PPU_MODE_DRAWING)
            return 0xFF;

        uint8_t cram_address = mmu->io_registers[IO_OBPI] & 0x3F;
        return mmu->cram_obj[cram_address];
    }
    case 0x6C ... 0x6F: return 0xFF;
    case IO_SVBK: return gb->cgb_mode_enabled ? 0xF8 | (mmu->io_registers[io_reg_addr] & 0x07) : 0xFF;
    case 0x71: return 0xFF;
    case 0x72 ... 0x73: return gb->base->opts.mode == GBMULATOR_MODE_GBC ? mmu->io_registers[io_reg_addr] : 0xFF;
    case 0x74: return gb->cgb_mode_enabled ? mmu->io_registers[io_reg_addr] : 0xFF;
    case 0x75: return gb->base->opts.mode == GBMULATOR_MODE_GBC ? mmu->io_registers[io_reg_addr] | 0x8F : 0xFF;
    case IO_PCM12:
    case IO_PCM34:
        // not emulated because it appears to be never used
        return gb->base->opts.mode == GBMULATOR_MODE_GBC ? 0x00 : 0xFF;
    case 0x78 ... 0x7F: return 0xFF;
    default:
        eprintf("invalid read at 0xFF%02X\n", io_reg_addr);
        exit(EXIT_FAILURE);
    }
}

static inline void write_io_register(gb_t *gb, uint8_t io_reg_addr, uint8_t data) {
    gb_mmu_t *mmu = gb->mmu;

    switch (io_reg_addr) {
    case IO_P1:
        // prevent writes to the lower nibble of the IO_P1 register (joypad)
        mmu->io_registers[io_reg_addr] = data & 0xF0;
        break;
    case IO_SC:
        if (CHECK_BIT(mmu->io_registers[io_reg_addr], 1) != CHECK_BIT(data, 1)) {
            mmu->io_registers[io_reg_addr] = data & 0x83;
            link_set_clock(gb);
        } else {
            mmu->io_registers[io_reg_addr] = data & 0x83;
        }
        break;
    case IO_DIV:
        // writing to DIV resets it to 0
        timer_set_div_timer(gb, 0);
        break;
    case IO_TIMA:
        if (gb->timer->tima_state == TIMA_LOADING) {
            gb->timer->tima_cancelled_value = data;
            gb->timer->tima_state = TIMA_LOADING_CANCELLED;
        } else if (gb->timer->tima_state != TIMA_LOADING_END) {
            mmu->io_registers[io_reg_addr] = data;
        }
        break;
    case IO_TMA:
        mmu->io_registers[io_reg_addr] = data;
        if (gb->timer->tima_state != TIMA_COUNTING)
            mmu->io_registers[IO_TIMA] = data;
        break;
    case IO_TAC:
        mmu->io_registers[io_reg_addr] = data;
        switch (data & 0x03) {
        case 0x00: gb->timer->tima_increase_div_bit = 9; break;
        case 0x01: gb->timer->tima_increase_div_bit = 3; break;
        case 0x02: gb->timer->tima_increase_div_bit = 5; break;
        case 0x03: gb->timer->tima_increase_div_bit = 7; break;
        }
        break;
    case IO_NR10:
        if (!IS_APU_ENABLED(gb))
            return;
        mmu->io_registers[io_reg_addr] = data;
        break;
    case IO_NR11:
        if (!IS_APU_ENABLED(gb))
            return;
        mmu->io_registers[io_reg_addr] = data;
        gb->apu->channels[0].length_counter = 64 - (data & 0x3F);
        break;
    case IO_NR12:
        if (!IS_APU_ENABLED(gb))
            return;
        mmu->io_registers[io_reg_addr] = data;
        if (!(data >> 3)) // if dac disabled
            APU_DISABLE_CHANNEL(gb, APU_CHANNEL_1);
        break;
    case IO_NR13:
        if (!IS_APU_ENABLED(gb))
            return;
        mmu->io_registers[io_reg_addr] = data;
        break;
    case IO_NR14:
        if (!IS_APU_ENABLED(gb))
            return;
        mmu->io_registers[io_reg_addr] = data;
        if (CHECK_BIT(data, 7) && (data >> 3)) // if trigger requested and dac enabled
            apu_channel_trigger(gb, &gb->apu->channels[0]);
        break;
    case IO_NR20:
        if (!IS_APU_ENABLED(gb))
            return;
        mmu->io_registers[io_reg_addr] = data;
        break;
    case IO_NR21:
        if (!IS_APU_ENABLED(gb))
            return;
        mmu->io_registers[io_reg_addr] = data;
        gb->apu->channels[1].length_counter = 64 - (data & 0x3F);
        break;
    case IO_NR22:
        if (!IS_APU_ENABLED(gb))
            return;
        mmu->io_registers[io_reg_addr] = data;
        if (!(data >> 3)) // if dac disabled
            APU_DISABLE_CHANNEL(gb, APU_CHANNEL_2);
        break;
    case IO_NR23:
        if (!IS_APU_ENABLED(gb))
            return;
        mmu->io_registers[io_reg_addr] = data;
        break;
    case IO_NR24:
        if (!IS_APU_ENABLED(gb))
            return;
        mmu->io_registers[io_reg_addr] = data;
        if (CHECK_BIT(data, 7) && (data >> 3)) // if trigger requested and dac enabled
            apu_channel_trigger(gb, &gb->apu->channels[1]);
        break;
    case IO_NR30:
        if (!IS_APU_ENABLED(gb))
            return;
        mmu->io_registers[io_reg_addr] = data;
        if (!(data >> 7)) // if dac disabled
            APU_DISABLE_CHANNEL(gb, APU_CHANNEL_3);
        break;
    case IO_NR31:
        if (!IS_APU_ENABLED(gb))
            return;
        mmu->io_registers[io_reg_addr] = data;
        gb->apu->channels[2].length_counter = 256 - data;
        break;
    case IO_NR32:
        if (!IS_APU_ENABLED(gb))
            return;
        mmu->io_registers[io_reg_addr] = data;
        break;
    case IO_NR33:
        if (!IS_APU_ENABLED(gb))
            return;
        mmu->io_registers[io_reg_addr] = data;
        break;
    case IO_NR34:
        if (!IS_APU_ENABLED(gb))
            return;
        mmu->io_registers[io_reg_addr] = data;
        if (CHECK_BIT(data, 7) && (data >> 7)) // if trigger requested and dac enabled
            apu_channel_trigger(gb, &gb->apu->channels[2]);
        break;
    case IO_NR40:
        if (!IS_APU_ENABLED(gb))
            return;
        mmu->io_registers[io_reg_addr] = data;
        break;
    case IO_NR41:
        if (!IS_APU_ENABLED(gb))
            return;
        mmu->io_registers[io_reg_addr] = data;
        gb->apu->channels[3].length_counter = 64 - (data & 0x3F);
        break;
    case IO_NR42:
        if (!IS_APU_ENABLED(gb))
            return;
        mmu->io_registers[io_reg_addr] = data;
        if (!(data >> 3)) // if dac disabled
            APU_DISABLE_CHANNEL(gb, APU_CHANNEL_4);
        break;
    case IO_NR43:
        if (!IS_APU_ENABLED(gb))
            return;
        mmu->io_registers[io_reg_addr] = data;
        break;
    case IO_NR44:
        if (!IS_APU_ENABLED(gb))
            return;
        mmu->io_registers[io_reg_addr] = data;
        if (CHECK_BIT(data, 7) && (data >> 3)) // if trigger requested and dac enabled
            apu_channel_trigger(gb, &gb->apu->channels[3]);
        break;
    case IO_NR50:
        if (!IS_APU_ENABLED(gb))
            return;
        mmu->io_registers[io_reg_addr] = data;
        break;
    case IO_NR51:
        if (!IS_APU_ENABLED(gb))
            return;
        mmu->io_registers[io_reg_addr] = data;
        break;
    case IO_NR52:
        CHANGE_BIT(mmu->io_registers[IO_NR52], 7, data >> 7);
        if (!IS_APU_ENABLED(gb))
            memset(&mmu->io_registers[IO_NR10], 0x00, 32 * sizeof(uint8_t)); // clear all registers
        break;
    case IO_WAVE_RAM ... IO_LCDC - 1:
        if (!CHECK_BIT(mmu->io_registers[IO_NR30], 7))
            mmu->io_registers[io_reg_addr] = data;
        break;
    case IO_LCDC: {
        uint8_t old_lcd_enabled = IS_LCD_ENABLED(gb);
        mmu->io_registers[io_reg_addr] = data;

        if (!old_lcd_enabled && IS_LCD_ENABLED(gb)) // lcd turned on
            ppu_enable_lcd(gb);
        else if (old_lcd_enabled && !IS_LCD_ENABLED(gb)) // lcd turned off
            ppu_disable_lcd(gb);
        break;
    }
    case IO_STAT: {
        // check for STAT interrupt only if in VBLANK, HBLANK or LY=LYC STAT bit is set
        // (not sure if the LY=LYC condition is accurate: this passes wilbertpol's stat_write_if-GS test
        // but maybe only due to other inaccuracies in the emulation)
        uint8_t update_irq_line =
                PPU_STAT_IS_MODE(gb, PPU_MODE_VBLANK) || PPU_STAT_IS_MODE(gb, PPU_MODE_HBLANK) ||
                CHECK_BIT(gb->mmu->io_registers[IO_STAT], 2);

        if (gb->base->opts.mode != GBMULATOR_MODE_GBC) {
            // on DMG hardware, writing any data to STAT is like writing 0xFF, then 4 cycles later, the actual data is written into STAT
            // the 4 cycles delay is not emulated because it this only relevant for STAT interrupts
            // so we check for a STAT interrupt then immediately set STAT to its actual value
            mmu->io_registers[io_reg_addr] = 0xF8 | (mmu->io_registers[io_reg_addr] & 0x07);

            if (update_irq_line)
                ppu_update_stat_irq_line(gb);
        }

        mmu->io_registers[io_reg_addr] =
                0x80 | (data & 0x78) | (mmu->io_registers[io_reg_addr] & 0x07);

        if (gb->base->opts.mode == GBMULATOR_MODE_GBC && update_irq_line)
            ppu_update_stat_irq_line(gb);
        break;
    }
    case IO_LY: break; // read only
    case IO_LYC:
        mmu->io_registers[IO_LYC] = data;
        if (IS_LCD_ENABLED(gb)) {
            UPDATE_STAT_LY_LYC_BIT(gb);
            ppu_update_stat_irq_line(gb);
        }
        break;
    case IO_DMA:
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
    case IO_KEY0:
        if (gb->base->opts.mode == GBMULATOR_MODE_GBC && !mmu->boot_finished) {
            gb->cgb_mode_enabled = !(data & 0x0C);
            mmu->io_registers[io_reg_addr] = data;
        }
        break;
    case IO_KEY1:
        mmu->io_registers[io_reg_addr] |= data & 0x01;
        break;
    case IO_VBK:
        if (gb->base->opts.mode == GBMULATOR_MODE_GBC) {
            mmu->io_registers[io_reg_addr] = data & 0x01;
            mmu->vram_bank_addr_offset = (GBC_CURRENT_VRAM_BANK(mmu) * VRAM_BANK_SIZE) - MMU_VRAM;
        }
        break;
    case IO_BANK:
        // disable boot rom
        if ((gb->base->opts.mode != GBMULATOR_MODE_GBC && data == 0x01) || (gb->base->opts.mode == GBMULATOR_MODE_GBC && data == 0x11))
            mmu->boot_finished = 1;
        mmu->io_registers[io_reg_addr] = data;
        break;
    case IO_HDMA5:
        if (!gb->cgb_mode_enabled)
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
            mmu->hdma.allow_hdma_block = gb->ppu->mode == PPU_MODE_HBLANK || !IS_LCD_ENABLED(gb);
        }

        mmu->hdma.src_address = ((mmu->io_registers[IO_HDMA1] << 8) | mmu->io_registers[IO_HDMA2]) & 0xFFF0;
        mmu->hdma.dest_address = 0x8000 | (((mmu->io_registers[IO_HDMA3] << 8) | mmu->io_registers[IO_HDMA4]) & 0x1FF0);

        mmu->hdma.initializing = 1;

        // if (mmu->hdma.type) // HBLANK DMA (HDMA)
        //     printf("HDMA size=%d (%d blocs), vram bank=%d, wram bank n=%d, src=%x, dest=%x\n", (mmu->io_registers[io_reg_addr] + 1) * 0x10, mmu->io_registers[io_reg_addr] + 1, GBC_CURRENT_VRAM_BANK(mmu), GBC_CURRENT_WRAM_BANK(mmu), mmu->hdma.src_address, mmu->hdma.dest_address);
        // else // General purpose DMA (GDMA)
        //     printf("GDMA size=%d (%d blocs), vram bank=%d, wram bank n=%d, src=%x, dest=%x\n", (mmu->io_registers[io_reg_addr] + 1) * 0x10, mmu->io_registers[io_reg_addr] + 1, GBC_CURRENT_VRAM_BANK(mmu), GBC_CURRENT_WRAM_BANK(mmu), mmu->hdma.src_address, mmu->hdma.dest_address);
        break;
    case IO_RP:
        mmu->io_registers[io_reg_addr] = gb->cgb_mode_enabled ? data & 0xC1 : 0xFF;
        break;
    case IO_BGPI:
        if (gb->base->opts.mode == GBMULATOR_MODE_GBC)
            mmu->io_registers[io_reg_addr] = data;
        break;
    case IO_BGPD:
        if (gb->base->opts.mode != GBMULATOR_MODE_GBC)
            break;

        if (gb->ppu->mode != PPU_MODE_DRAWING) {
            uint8_t cram_address = mmu->io_registers[IO_BGPI] & 0x3F;
            mmu->cram_bg[cram_address] = data;
            // printf("write %d in cram_bg %d\n", data, cram_address);
        }

        // increment BGPI address if auto increment (bit.7) of BGPI is set
        if (CHECK_BIT(mmu->io_registers[IO_BGPI], 7)) {
            uint8_t new_bgpi_address = (mmu->io_registers[IO_BGPI] & 0x3F) + 1;
            if (new_bgpi_address > 0x3F)
                new_bgpi_address = 0;
            mmu->io_registers[IO_BGPI] = new_bgpi_address;
            SET_BIT(mmu->io_registers[IO_BGPI], 7);
        }
        break;
    case IO_OBPI:
        if (gb->base->opts.mode == GBMULATOR_MODE_GBC)
            mmu->io_registers[io_reg_addr] = data;
        break;
    case IO_OBPD:
        if (gb->base->opts.mode != GBMULATOR_MODE_GBC)
            break;

        if (gb->ppu->mode != PPU_MODE_DRAWING) {
            uint8_t cram_address = mmu->io_registers[IO_OBPI] & 0x3F;
            mmu->cram_obj[cram_address] = data;
            // printf("write %d in cram_obj %d\n", data, cram_address);
        }

        // increment OBPI address if auto increment (bit.7) of OBPI is set
        if (CHECK_BIT(mmu->io_registers[IO_OBPI], 7)) {
            uint8_t new_obpi_address = (mmu->io_registers[IO_OBPI] & 0x3F) + 1;
            if (new_obpi_address > 0x3F)
                new_obpi_address = 0;
            mmu->io_registers[IO_OBPI] = new_obpi_address;
            SET_BIT(mmu->io_registers[IO_OBPI], 7);
        }
        break;
    case IO_SVBK:
        if (gb->cgb_mode_enabled) {
            mmu->io_registers[io_reg_addr] = data & 0x07;
            mmu->wram_bankn_addr_offset = ((GBC_CURRENT_WRAM_BANK(mmu) - 1) * WRAM_BANK_SIZE) - MMU_WRAM_BANK0;
        }
        break;
    case 0x74:
        if (gb->base->opts.mode == GBMULATOR_MODE_GBC)
            mmu->io_registers[io_reg_addr] = data; // only writable in CGB mode
        break;
    case 0x75:
        mmu->io_registers[io_reg_addr] = gb->base->opts.mode == GBMULATOR_MODE_GBC ? data & 0x70 : data;
        break;
    case 0x76:
        if (gb->base->opts.mode != GBMULATOR_MODE_GBC)
            mmu->io_registers[io_reg_addr] = data; // only writable in DMG mode
        break;
    case 0x77:
        if (gb->base->opts.mode != GBMULATOR_MODE_GBC)
            mmu->io_registers[io_reg_addr] = data; // only writable in DMG mode
        break;
    default:
        mmu->io_registers[io_reg_addr] = data;
        break;
    }
}

uint8_t mmu_read_io_src(gb_t *gb, uint16_t address, gb_io_source_t io_src) {
    gb_mmu_t *mmu = gb->mmu;

    switch (address & 0xF000) {
    case MMU_ROM_BANK0:
        if (!mmu->boot_finished) {
            if (gb->base->opts.mode != GBMULATOR_MODE_GBC && address < 0x100)
                return mmu->dmg_boot_rom[address];
            if (gb->base->opts.mode == GBMULATOR_MODE_GBC && (address < 0x100 || (address >= 0x200 && address < sizeof(cgb_boot))))
                return mmu->cgb_boot_rom[address];
        }
        // fallthrough
    case MMU_ROM_BANK0 + 0x1000:
    case MMU_ROM_BANK0 + 0x2000:
    case MMU_ROM_BANK0 + 0x3000:
        return mmu->rom[mmu->rom_bank0_addr + address];
    case MMU_ROM_BANKN:
    case MMU_ROM_BANKN + 0x1000:
    case MMU_ROM_BANKN + 0x2000:
    case MMU_ROM_BANKN + 0x3000:
        return mmu->rom[mmu->rom_bankn_addr + address];
    case MMU_VRAM:
    case MMU_VRAM + 0x1000:
        if (io_src == IO_SRC_GDMA_HDMA)
            return 0xFF; // src can be inside vram but it reads incorrect data: see TCAGBD.pdf for more details
        if (io_src == IO_SRC_CPU && is_vram_locked_for_cpu_read(gb))
            return 0xFF;
        return mmu->vram[mmu->vram_bank_addr_offset + address];
    case MMU_ERAM:
    case MMU_ERAM + 0x1000:
        return mbc_read_eram(gb, address);
    case MMU_WRAM_BANK0:
        return mmu->wram[address - MMU_WRAM_BANK0];
    case MMU_WRAM_BANKN:
        return mmu->wram[mmu->wram_bankn_addr_offset + address];
    case MMU_ECHO:
        return mmu->wram[address - MMU_ECHO];
    case 0xF000:
        // From CasualPokePlayer (https://github.com/skylersaleh/SkyEmu/blob/52a08105c06f1a4f1e9215b2f9ab83fe04ee6236/src/gb.h#L1267):
        // in most cases echo ram is only E000-FDFF. 
        // oam dma is one of the exceptions here which have the entire E000-FFFF
        // region as echo ram for dma source (therefore only for memory reads by the oam dma)
        if (address < MMU_OAM || io_src == IO_SRC_OAM_DMA || io_src == IO_SRC_GDMA_HDMA) // we are still in ECHO ram
            return mmu->wram[(mmu->wram_bankn_addr_offset - (MMU_ECHO - MMU_WRAM_BANK0)) + address];

        if (address < MMU_UNUSABLE) { // we are in MMU_OAM
            if (io_src == IO_SRC_CPU && is_oam_locked_for_cpu_read(gb))
                return 0xFF;
            return mmu->oam[address - MMU_OAM];
        }

        if (address < MMU_IO) // we are in MMU_UNUSABLE
            return 0xFF;

        if (address < MMU_HRAM)
            return read_io_register(gb, address & 0x00FF);

        if (address < MMU_IE)
            return mmu->hram[address - MMU_HRAM];

        return mmu->ie;
    default:
        eprintf("invalid cpu read at address 0x%X\n", address);
        exit(EXIT_FAILURE);
    }
}

void mmu_write_io_src(gb_t *gb, uint16_t address, uint8_t data, gb_io_source_t io_src) {
    gb_mmu_t *mmu = gb->mmu;

    switch (address & 0xF000) {
    case MMU_ROM_BANK0:
    case MMU_ROM_BANK0 + 0x1000:
    case MMU_ROM_BANK0 + 0x2000:
    case MMU_ROM_BANK0 + 0x3000:
    case MMU_ROM_BANKN:
    case MMU_ROM_BANKN + 0x1000:
    case MMU_ROM_BANKN + 0x2000:
    case MMU_ROM_BANKN + 0x3000:
        mbc_write_registers(gb, address, data);
        break;
    case MMU_VRAM:
    case MMU_VRAM + 0x1000:
        if (io_src == IO_SRC_CPU && is_vram_locked_for_cpu_write(gb))
            break;
        mmu->vram[mmu->vram_bank_addr_offset + address] = data;
        break;
    case MMU_ERAM:
    case MMU_ERAM + 0x1000:
        mbc_write_eram(gb, address, data);
        break;
    case MMU_WRAM_BANK0:
        mmu->wram[address - MMU_WRAM_BANK0] = data;
        break;
    case MMU_WRAM_BANKN:
        mmu->wram[mmu->wram_bankn_addr_offset + address] = data;
        break;
    case MMU_ECHO:
        mmu->wram[address - MMU_ECHO] = data;
        break;
    case 0xF000:
        if (address < MMU_OAM) { // we are still in echo ram
            mmu->wram[(mmu->wram_bankn_addr_offset - (MMU_ECHO - MMU_WRAM_BANK0)) + address] = data;
        } else if (address < MMU_UNUSABLE) {
            if (io_src == IO_SRC_CPU && is_oam_locked_for_cpu_write(gb))
                break;
            mmu->oam[address - MMU_OAM] = data;
        } else if (address < MMU_IO) {
            // MMU_UNUSABLE memory is unusable
        } else if (address < MMU_HRAM) {
            write_io_register(gb, address & 0x00FF, data);
        } else if (address < MMU_IE) {
            mmu->hram[address - MMU_HRAM] = data;
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
#define SERIALIZED_MEMBERS                                                                     \
    X(rom_size)                                                                                \
    Y(vram, gb->base->opts.mode == GBMULATOR_MODE_GBC, 2 * VRAM_BANK_SIZE, VRAM_BANK_SIZE)     \
    Z(eram, eram_banks, ERAM_BANK_SIZE)                                                        \
    Y(wram, gb->base->opts.mode == GBMULATOR_MODE_GBC, 8 * WRAM_BANK_SIZE, 2 * WRAM_BANK_SIZE) \
    X(oam)                                                                                     \
    X(io_registers)                                                                            \
    X(hram)                                                                                    \
    X(ie)                                                                                      \
    W(cram_bg)                                                                                 \
    W(cram_obj)                                                                                \
    X(boot_finished)                                                                           \
    X(hdma.initializing)                                                                       \
    X(hdma.allow_hdma_block)                                                                   \
    X(hdma.lock_cpu)                                                                           \
    X(hdma.type)                                                                               \
    X(hdma.progress)                                                                           \
    X(hdma.src_address)                                                                        \
    X(hdma.dest_address)                                                                       \
    X(oam_dma.starting_statuses)                                                               \
    X(oam_dma.starting_count)                                                                  \
    X(oam_dma.progress)                                                                        \
    X(oam_dma.src_address)                                                                     \
    X(vram_bank_addr_offset)                                                                   \
    X(wram_bankn_addr_offset)                                                                  \
    X(rom_banks)                                                                               \
    X(eram_banks)                                                                              \
    X(rom_bank0_addr)                                                                          \
    X(rom_bankn_addr)                                                                          \
    X(eram_bank_addr)                                                                          \
    X(has_battery)                                                                             \
    X(has_rumble)                                                                              \
    X(has_rtc)

#define X(value) SERIALIZED_LENGTH(value);
#define Y(...) SERIALIZED_LENGTH_COND_LITERAL(__VA_ARGS__);
#define Z(...) SERIALIZED_LENGTH_FROM_MEMBER(__VA_ARGS__);
#define W(...) SERIALIZED_LENGTH_IF_CGB(__VA_ARGS__);
SERIALIZED_SIZE_FUNCTION(gb_mmu_t, mmu,
    SERIALIZED_MEMBERS
    {
        gb_mbc_t *tmp = &gb->mmu->mbc;
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
SERIALIZER_FUNCTION(gb_mmu_t, mmu,
    SERIALIZED_MEMBERS
    {
        gb_mbc_t *tmp = &gb->mmu->mbc;
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
UNSERIALIZER_FUNCTION(gb_mmu_t, mmu,
    SERIALIZED_MEMBERS
    {
        gb_mbc_t *tmp = &gb->mmu->mbc;
        MBC_SERIALIZED_MEMBERS
    }
)
#undef W
#undef Z
#undef Y
#undef X
