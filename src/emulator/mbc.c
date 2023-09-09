#include "emulator_priv.h"
#include "serialize.h"

#define IS_RTC_HALTED(mbc) ((mbc)->mbc3.rtc.dh & 0x40)

#define EEPROM_DO(pins) ((pins) & 0x01) // MBC7 EEPROM DO pin
#define EEPROM_DI(pins) ((pins) & 0x02) // MBC7 EEPROM DI pin
#define EEPROM_CLK(pins) ((pins) & 0x40) // MBC7 EEPROM CLK pin
#define EEPROM_CS(pins) ((pins) & 0x80) // MBC7 EEPROM CS pin

static inline void write_mbc7_eeprom(mbc_t *mbc, byte_t data) {
    if (!EEPROM_CS(data)) { // if CS (Chip Select) not set, only update pins
        byte_t bit_do = mbc->mbc7.eeprom.output_bits >> 15;
        mbc->mbc7.eeprom.pins = (data & 0xC2) | bit_do;
        return;
    }

    if (!EEPROM_CLK(mbc->mbc7.eeprom.pins) && EEPROM_CLK(data)) { // if go from CLK 0 (Clock) to CLK 1, we just clocked
        // shift out DO
        mbc->mbc7.eeprom.output_bits <<= 1;

        if (mbc->mbc7.eeprom.command_arg_remaining_bits) {
            mbc->mbc7.eeprom.output_bits = 0;
            // shift in DI to build argument for WRITE or WRAL
            mbc->mbc7.eeprom.command_arg_remaining_bits--;
            if (EEPROM_DI(data) && mbc->mbc7.eeprom.write_enabled) {
                if (mbc->mbc7.eeprom.command & 0x100) {
                    // WRITE
                    SET_BIT(mbc->mbc7.eeprom.data[mbc->mbc7.eeprom.command & 0x7F], mbc->mbc7.eeprom.command_arg_remaining_bits);
                } else {
                    // WRAL
                    for (byte_t i = 0; i < 0x7F; i++)
                        SET_BIT(mbc->mbc7.eeprom.data[i], mbc->mbc7.eeprom.command_arg_remaining_bits);
                }
            }

            if (!mbc->mbc7.eeprom.command_arg_remaining_bits) {
                mbc->mbc7.eeprom.command = 0;
                // Set next bit to be shifted out to 1. This makes DO == 1 now, thus settle time is not emulated (not important).
                // To emulate settle time we could place the set bit in one of the first 15 bit of 'output_bits'.
                mbc->mbc7.eeprom.output_bits = 0x8000;
            }
        } else {
            // shift in DI to build command
            mbc->mbc7.eeprom.command <<= 1;
            mbc->mbc7.eeprom.command |= (EEPROM_DI(data) >> 1);

            if (mbc->mbc7.eeprom.command & 0x0400) { // valid command if bit 11 is set (start bit)
                word_t stripped_command = mbc->mbc7.eeprom.command & 0x03FF; // remove start bit from command

                switch ((stripped_command >> 6) & 0x000F) {
                case 0x00: // EWDS
                    mbc->mbc7.eeprom.write_enabled = 0;
                    mbc->mbc7.eeprom.command = 0;
                    break;
                case 0x01: // WRAL
                    if (mbc->mbc7.eeprom.write_enabled)
                        memset(mbc->mbc7.eeprom.data, 0, sizeof(mbc->mbc7.eeprom.data));
                    mbc->mbc7.eeprom.command_arg_remaining_bits = 16;
                    // don't set command to 0 yet as we still need it after its arguments has been shifted in
                    break;
                case 0x02: // ERAL
                    if (mbc->mbc7.eeprom.write_enabled)
                        memset(mbc->mbc7.eeprom.data, 0xFF, sizeof(mbc->mbc7.eeprom.data));
                    mbc->mbc7.eeprom.command = 0;
                    break;
                case 0x03: // EWEN
                    mbc->mbc7.eeprom.write_enabled = 1;
                    mbc->mbc7.eeprom.command = 0;
                    break;
                case 0x04 ... 0x07: // WRITE
                    if (mbc->mbc7.eeprom.write_enabled)
                        mbc->mbc7.eeprom.data[mbc->mbc7.eeprom.command & 0x7F] = 0;
                    mbc->mbc7.eeprom.command_arg_remaining_bits = 16;
                    // don't set command to 0 yet as we still need it after its arguments has been shifted in
                    break;
                case 0x08 ... 0x0B: { // READ
                    word_t eeprom_address = stripped_command & 0x7F;
                    mbc->mbc7.eeprom.output_bits = ((mbc->mbc7.eeprom.data[eeprom_address + 1]) << 8) | mbc->mbc7.eeprom.data[eeprom_address];
                    mbc->mbc7.eeprom.command = 0;
                } break;
                case 0x0C ... 0x0F: // ERASE
                    if (mbc->mbc7.eeprom.write_enabled) {
                        mbc->mbc7.eeprom.data[mbc->mbc7.eeprom.command & 0x7F] = 0xFF;
                        mbc->mbc7.eeprom.data[(mbc->mbc7.eeprom.command & 0x7F) + 1] = 0xFF;
                    }
                    mbc->mbc7.eeprom.command = 0;
                    break;
                }
            }
        }
    }

    byte_t bit_do = mbc->mbc7.eeprom.output_bits >> 15;
    mbc->mbc7.eeprom.pins = (data & 0xC2) | bit_do;
}

static inline void mbc1_set_bank_addrs(mmu_t *mmu, byte_t bank1_size) {
    mbc_t *mbc = &mmu->mbc;

    if (mbc->mbc1.mode) { // ERAM mode
        byte_t current_rom_bank0 = (mbc->mbc1.bank_hi << bank1_size) & (mmu->rom_banks - 1);
        mmu->rom_bank0_addr = current_rom_bank0 * ROM_BANK_SIZE;

        byte_t eram_bank = mbc->mbc1.bank_hi & (mmu->eram_banks - 1);
        mmu->eram_bank_addr = eram_bank * ERAM_BANK_SIZE;
    } else { // ROM mode
        // NOTE: this is not really a BANK0 pointer: it's actually a BANKN pointer that happens to be mapped
        // in the memory address region ROM_BANK0. (IS THIS STILL TRUE? --> read docs)
        mmu->rom_bank0_addr = 0;
        mmu->eram_bank_addr = 0;
    }

    byte_t current_rom_bankn = (mbc->mbc1.bank_hi << bank1_size) | mbc->mbc1.bank_lo;
    current_rom_bankn &= mmu->rom_banks - 1;

    mmu->rom_bankn_addr = (current_rom_bankn - 1) * ROM_BANK_SIZE; // -1 to add the -ROM_BANK_SIZE offset
}

void mbc_write_registers(emulator_t *emu, word_t address, byte_t data) {
    mmu_t *mmu = emu->mmu;
    mbc_t *mbc = &mmu->mbc;

    switch (mbc->type) {
    case ROM_ONLY:
        break; // read only, do nothing
    case MBC1:
        switch (address & 0xE000) {
        case 0x0000:
            mbc->eram_enabled = mmu->eram_banks > 0 && (data & 0x0F) == 0x0A;
            break;
        case 0x2000:
            data &= 0x1F;
            mbc->mbc1.bank_lo = data == 0x00 ? 0x01 : data; // BANK1 can't be 0
            mbc1_set_bank_addrs(mmu, 5);
            break;
        case 0x4000:
            data &= 0x03;
            mbc->mbc1.bank_hi = data;
            mbc1_set_bank_addrs(mmu, 5);
            break;
        case 0x6000:
            mbc->mbc1.mode = data & 0x01;
            mbc1_set_bank_addrs(mmu, 5);
            break;
        }
        break;
    case MBC1M:
        switch (address & 0xE000) {
        case 0x0000:
            mbc->eram_enabled = mmu->eram_banks > 0 && (data & 0x0F) == 0x0A;
            break;
        case 0x2000:
            data &= 0x1F;
            mbc->mbc1.bank_lo = data == 0x00 ? 0x01 : data; // BANK1 can't be 0
            mbc->mbc1.bank_lo &= 0x0F; // MBC1M discards bit 4 of BANK1 register
            mbc1_set_bank_addrs(mmu, 4);
            break;
        case 0x4000:
            data &= 0x03;
            mbc->mbc1.bank_hi = data;
            mbc1_set_bank_addrs(mmu, 4);
            break;
        case 0x6000:
            mbc->mbc1.mode = data & 0x01;
            mbc1_set_bank_addrs(mmu, 4);
            break;
        }
        break;
    case MBC2:
        if (address >= 0x4000)
            break;

        if (!CHECK_BIT(address, 8)) {
            mbc->eram_enabled = (data & 0x0F) == 0x0A;
        } else {
            data &= 0x0F;
            mbc->mbc2.rom_bank = data == 0x00 ? 0x01 : data; // BANK1 can't be 0
            mbc->mbc2.rom_bank &= mmu->rom_banks - 1; // in this case, equivalent to current_rom_bank %= rom_banks but avoid division by 0
            mmu->rom_bankn_addr = (mbc->mbc2.rom_bank - 1) * ROM_BANK_SIZE; // -1 to add the -ROM_BANK_SIZE offset
        }
        break;
    case MBC3:
    case MBC30:
        switch (address & 0xE000) {
        case 0x0000:
            mbc->eram_enabled = (data & 0x0F) == 0x0A;
            if (mmu->has_rtc)
                mbc->mbc3.rtc.enabled = mbc->eram_enabled;
            break;
        case 0x2000:
            mbc->mbc3.rom_bank = mbc->type == MBC30 ? data : data & 0x7F;
            if (mbc->mbc3.rom_bank == 0x00)
                mbc->mbc3.rom_bank = 0x01; // 0x00 not allowed
            mbc->mbc3.rom_bank &= mmu->rom_banks - 1; // in this case, equivalent to mbc->mbc3.rom_bank %= rom_banks but avoid division by 0
            mmu->rom_bankn_addr = (mbc->mbc3.rom_bank - 1) * ROM_BANK_SIZE; // -1 to add the -ROM_BANK_SIZE offset
            break;
        case 0x4000:;
            byte_t max_ram_bank = mbc->type == MBC30 ? 0x07 : 0x03;
            if (data <= max_ram_bank) {
                mbc->mbc3.eram_bank = data;
                mbc->mbc3.eram_bank &= mmu->eram_banks - 1; // in this case, equivalent to mbc->mbc3.eram_bank %= eram_banks but avoid division by 0
                mmu->eram_bank_addr = mbc->mbc3.eram_bank * ERAM_BANK_SIZE;
                mbc->mbc3.rtc_mapped = 0;
            } else if (mmu->has_rtc && data >= 0x08 && data <= 0x0C) {
                mbc->mbc3.rtc.reg = data;
                mbc->mbc3.rtc_mapped = 1;
            }
            break;
        case 0x6000:
            if (mbc->mbc3.rtc.latch == 0x00 && data == 0x01) {
                mbc->mbc3.rtc.latched_s = mbc->mbc3.rtc.s;
                mbc->mbc3.rtc.latched_m = mbc->mbc3.rtc.m;
                mbc->mbc3.rtc.latched_h = mbc->mbc3.rtc.h;
                mbc->mbc3.rtc.latched_dl = mbc->mbc3.rtc.dl;
                mbc->mbc3.rtc.latched_dh = mbc->mbc3.rtc.dh;
            }
            mbc->mbc3.rtc.latch = data;
            break;
        }
        break;
    case MBC5:
        switch (address & 0xF000) {
        case 0x0000:
        case 0x1000:
            mbc->eram_enabled = data == 0x0A;
            break;
        case 0x2000: {
            mbc->mbc5.rom_bank_lo = data;
            word_t current_rom_bank = (mbc->mbc5.rom_bank_hi << 8) | mbc->mbc5.rom_bank_lo;
            current_rom_bank &= mmu->rom_banks - 1; // in this case, equivalent to current_rom_bank %= rom_banks but avoid division by 0
            mmu->rom_bankn_addr = (current_rom_bank - 1) * ROM_BANK_SIZE; // -1 to add the -ROM_BANK_SIZE offset
            break;
        }
        case 0x3000: {
            mbc->mbc5.rom_bank_hi = data;
            word_t current_rom_bank = (mbc->mbc5.rom_bank_hi << 8) | mbc->mbc5.rom_bank_lo;
            current_rom_bank &= mmu->rom_banks - 1; // in this case, equivalent to current_rom_bank %= rom_banks but avoid division by 0
            mmu->rom_bankn_addr = (current_rom_bank - 1) * ROM_BANK_SIZE; // -1 to add the -ROM_BANK_SIZE offset
            break;
        }
        case 0x4000:
        case 0x5000:
            mbc->mbc5.eram_bank = data & 0x0F;
            mbc->mbc5.eram_bank &= mmu->eram_banks - 1; // in this case, equivalent to mbc->mbc5.eram_bank %= eram_banks but avoid division by 0
            mmu->eram_bank_addr = mbc->mbc5.eram_bank * ERAM_BANK_SIZE;
            break;
        }
        break;
    case MBC7:
        switch (address & 0xE000) {
        case 0x0000:
            mbc->eram_enabled = data == 0x0A;
            break;
        case 0x2000:
            mbc->mbc7.rom_bank = data;
            mbc->mbc7.rom_bank &= mmu->rom_banks - 1; // in this case, equivalent to current_rom_bank %= rom_banks but avoid division by 0
            mmu->rom_bankn_addr = (mbc->mbc7.rom_bank - 1) * ROM_BANK_SIZE; // -1 to add the -ROM_BANK_SIZE offset
            break;
        case 0x4000:
            mbc->mbc7.eram_enabled2 = data == 0x40;
            break;
        }
        break;
    case HuC1:
        switch (address & 0xE000) {
        case 0x0000:
            mbc->huc1.ir_mode = (data & 0x0F) == 0x0E;
            mbc->eram_enabled = !mbc->huc1.ir_mode;
            break;
        case 0x2000:
            mbc->huc1.rom_bank = data;
            mbc->huc1.rom_bank &= mmu->rom_banks - 1; // in this case, equivalent to current_rom_bank %= rom_banks but avoid division by 0
            mmu->rom_bankn_addr = (mbc->huc1.rom_bank - 1) * ROM_BANK_SIZE; // -1 to add the -ROM_BANK_SIZE offset
            break;
        case 0x4000:
            mbc->huc1.eram_bank = data;
            mbc->huc1.eram_bank &= mmu->eram_banks - 1; // in this case, equivalent to mbc->huc1.eram_bank %= eram_banks but avoid division by 0
            mmu->eram_bank_addr = mbc->huc1.eram_bank * ERAM_BANK_SIZE;
            break;
        }
        break;
    }
}

byte_t mbc_read_eram(emulator_t *emu, word_t address) {
    mmu_t *mmu = emu->mmu;
    mbc_t *mbc = &mmu->mbc;

    if (mbc->type == MBC7) {
        // both mbc7 ram enable registers must be set and everything from ERAM + 0x1000 is unused (always reads 0xFF)
        if (!mbc->eram_enabled || !mbc->mbc7.eram_enabled2 || address >= ERAM + 0x1000)
            return 0xFF;

        switch (address & 0x00F0) {
        case 0x0020 /* 0xAx2x */:
            // printf("read accel x low: 0x%02X\n", mbc->mbc7.accelerometer.latched_x & 0xFF);
            return mbc->mbc7.accelerometer.latched_x; // accelerometer x low (read only)
        case 0x0030 /* 0xAx3x */:
            // printf("read accel x high: 0x%02X\n", mbc->mbc7.accelerometer.latched_x >> 8);
            return mbc->mbc7.accelerometer.latched_x >> 8; // accelerometer x high (read only)
        case 0x0040 /* 0xAx4x */:
            // printf("read accel y low: 0x%02X\n", mbc->mbc7.accelerometer.latched_y & 0xFF);
            return mbc->mbc7.accelerometer.latched_y; // accelerometer y low (read only)
        case 0x0050 /* 0xAx5x */:
            // printf("read accel y high: 0x%02X\n", mbc->mbc7.accelerometer.latched_y >> 8);
            return mbc->mbc7.accelerometer.latched_y >> 8; // accelerometer y high (read only)
        case 0x0060 /* 0xAx6x */: return 0x00; // unused (always reads 0x00)
        case 0x0080 /* 0xAx8x */: return mbc->mbc7.eeprom.pins;
        default: return 0xFF; // 0xAx0x -> 0xAx1x are write only, 0xAx7x and 0xAx9x -> 0xAxFx are unused and always reads 0xFF
        }
    }

    if (mbc->type == HuC1 && mbc->huc1.ir_mode) {
        // TODO
        eprintf("TODO read HuC1 IR light register (read 0xC1 --> seen light, 0xC0 --> did not see light)\n");
        return 0xC0;
    }

    byte_t can_access_rtc = (mbc->type == MBC3 || mbc->type == MBC30) && mbc->mbc3.rtc_mapped; // mbc->mbc3.rtc_mapped implies that mmu->has_rtc is true
    if (mbc->eram_enabled && !can_access_rtc) {
        if (mbc->type == MBC2) {
            // wrap around from 0xA200 to 0xBFFF (eg: address 0xA200 reads as address 0xA000)
            // return mmu->eram_bank_pointer[ERAM + (address & 0x1FF)] | 0xF0;
            return mmu->eram[mmu->eram_bank_addr + ((address - ERAM) & 0x1FF)] | 0xF0;
        } else {
            return mmu->eram[mmu->eram_bank_addr + (address - ERAM)];
        }
    }

    if (mbc->mbc3.rtc.enabled) { // mbc->mbc3.rtc.enabled implies that mmu->has_rtc is true
        switch (mbc->mbc3.rtc.reg) {
        case 0x08: return mbc->mbc3.rtc.latched_s & 0x3F;
        case 0x09: return mbc->mbc3.rtc.latched_m & 0x3F;
        case 0x0A: return mbc->mbc3.rtc.latched_h & 0x1F;
        case 0x0B: return mbc->mbc3.rtc.latched_dl;
        case 0x0C: return mbc->mbc3.rtc.latched_dh & 0xC1;
        default: return 0xFF;
        }
    }

    return 0xFF;
}

void mbc_write_eram(emulator_t *emu, word_t address, byte_t data) {
    mmu_t *mmu = emu->mmu;
    mbc_t *mbc = &mmu->mbc;

    if (mbc->type == MBC7) {
        // both mbc7 ram enable registers must be set and everything from ERAM + 0x1000 is unused
        if (!mbc->eram_enabled || !mbc->mbc7.eram_enabled2 || address >= ERAM + 0x1000)
            return;

        switch (address & 0x00F0) {
        case 0x0000 /* 0xAx0x */: // erase latched accelerometer (write only)
            if (data == 0x55) {
                mbc->mbc7.accelerometer.latched_x = 0x8000;
                mbc->mbc7.accelerometer.latched_y = 0x8000;
                mbc->mbc7.accelerometer.latch_ready = 1;
            }
            return;
        case 0x0010 /* 0xAx1x */: // latch accelerometer (write only)
            if (data == 0xAA && mbc->mbc7.accelerometer.latch_ready) {
                // TODO accelerometer is buggy (see kirby tilt n tumble)
                double x = 0.0f;
                double y = 0.0f;
                if (emu->on_accelerometer_request)
                    emu->on_accelerometer_request(&x, &y);
                mbc->mbc7.accelerometer.latched_x = 0x81D0 + (0x70 * x); // accelerometer_center + gravity * x
                mbc->mbc7.accelerometer.latched_y = 0x81D0 + (0x70 * y); // accelerometer_center + gravity * y
                mbc->mbc7.accelerometer.latch_ready = 0;
            }
            return;
        case 0x0080 /* 0xAx8x */:
            write_mbc7_eeprom(mbc, data);
            return;
        default: return; // 0xAx2x -> 0xAx5x are read only, 0xAx6x -> 0xAx7x and 0xAx9x -> 0xAxFx are unused
        }
    }

    if (mbc->type == HuC1 && mbc->huc1.ir_mode) {
        // TODO
        eprintf("TODO write 0x%02X in HuC1 IR light register (write 0x01 --> turn on light, 0x00 --> turn off light)\n", data);
        return;
    }

    byte_t can_access_rtc = (mbc->type == MBC3 || mbc->type == MBC30) && mbc->mbc3.rtc_mapped; // mbc->mbc3.rtc_mapped implies that mmu->has_rtc is true
    if (mbc->eram_enabled && !can_access_rtc) {
        mmu->eram[mmu->eram_bank_addr + (address - ERAM)] = data;
    } else if (mbc->mbc3.rtc.enabled) { // mbc->mbc3.rtc.enabled implies that mmu->has_rtc is true
        switch (mbc->mbc3.rtc.reg) {
        case 0x08:
            mbc->mbc3.rtc.s = data & 0x3F;
            mbc->mbc3.rtc.rtc_cycles = 0; // writing to the seconds register apparently resets the rtc's internal counter (from rtc3test rom)
            return;
        case 0x09: mbc->mbc3.rtc.m = data & 0x3F; return;
        case 0x0A: mbc->mbc3.rtc.h = data & 0x1F; return;
        case 0x0B: mbc->mbc3.rtc.dl = data; return;
        case 0x0C: mbc->mbc3.rtc.dh = data& 0xC1; return;
        default: return;
        }
    }
}

void rtc_step(emulator_t *emu) {
    mbc_t *mbc = &emu->mmu->mbc;

    if (IS_RTC_HALTED(mbc))
        return;

    // rtc internal clock should increase at 32768 Hz but just updating it once per emulated second
    // passes all of the tests of the rtc3test rom.
    // This may be because no time register changes that fast (as the smallest unit is the second).
    mbc->mbc3.rtc.rtc_cycles += 4;
    if (mbc->mbc3.rtc.rtc_cycles < GB_CPU_FREQ)
        return;
    mbc->mbc3.rtc.rtc_cycles = 0;

    mbc->mbc3.rtc.s++;
    if (mbc->mbc3.rtc.s > 0x3F) {
        mbc->mbc3.rtc.s = 0;
        return;
    }
    if (mbc->mbc3.rtc.s != 60)
        return;
    mbc->mbc3.rtc.s = 0;

    mbc->mbc3.rtc.m++;
    if (mbc->mbc3.rtc.m > 0x3F) {
        mbc->mbc3.rtc.m = 0;
        return;
    }
    if (mbc->mbc3.rtc.m != 60)
        return;
    mbc->mbc3.rtc.m = 0;

    mbc->mbc3.rtc.h++;
    if (mbc->mbc3.rtc.h > 0x1F) {
        mbc->mbc3.rtc.h = 0;
        return;
    }
    if (mbc->mbc3.rtc.h != 24)
        return;
    mbc->mbc3.rtc.h = 0;

    word_t d = ((mbc->mbc3.rtc.dh & 0x01) << 8) | mbc->mbc3.rtc.dl;
    d++;
    mbc->mbc3.rtc.dl = d & 0x00FF;
    if (CHECK_BIT(d, 8))
        SET_BIT(mbc->mbc3.rtc.dh, 0);
    else
        RESET_BIT(mbc->mbc3.rtc.dh, 0);
    if (d < 512)
        return;

    mbc->mbc3.rtc.dl = 0;
    RESET_BIT(mbc->mbc3.rtc.dh, 0);
    SET_BIT(mbc->mbc3.rtc.dh, 7); // set overflow bit
}
