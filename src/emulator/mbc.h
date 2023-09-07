#pragma once

#include "emulator_priv.h"

typedef enum {
    ROM_ONLY,
    MBC1,
    MBC1M,
    MBC2,
    MBC3,
    MBC30,
    MBC5,
    MBC6,
    MBC7,
    MMM01,
    M161,
    HuC1,
    HuC3,
    CAMERA,
    TAMA5
} mbc_type_t;

typedef enum {
    HuC3_RAM_RO,
    HuC3_RAM_RW = 0x0A,
    HuC3_RTC_CMD_ARG = 0x0B,
    HuC3_RTC_CMD_RES = 0x0C,
    HuC3_RTC_SEMAPHORE = 0x0D,
    HuC3_IR = 0x0E,
    HuC3_NONE,
} huc3_mode_t;

typedef struct {
    byte_t type;
    byte_t eram_enabled; // current eram bank number (shared by all MBCs)

    union {
        struct {
            byte_t mode;
            byte_t bank_lo;
            byte_t bank_hi;
        } mbc1;

        struct {
            byte_t rom_bank;
        } mbc2;

        struct {
            byte_t rtc_mapped;
            byte_t rom_bank;
            byte_t eram_bank;
        } mbc3;

        struct {
            uint8_t rom_bank_lo;
            uint8_t rom_bank_hi;
            uint8_t eram_bank;
        } mbc5;

        struct {
            byte_t eram_enabled2; // both mbc.eram_enabled and mbc.mbc7.eram_enabled2 must be 1 to enable eeprom
            byte_t rom_bank;

            struct {
                word_t latched_x;
                word_t latched_y;
                byte_t latch_ready; // 1 if latch was just erased and not yet relatched, else 0
            } accelerometer;

            struct {
                byte_t data[256]; // 2048 bits addressed 16 bits at a time (addresses are 7 bits wide)
                byte_t pins;
                word_t command;
                word_t command_arg_remaining_bits; // how many bits of the WRITE or WRAL command argument remains to be shifted in
                word_t output_bits;
                byte_t write_enabled;
            } eeprom;
        } mbc7;

        struct {
            byte_t rom_bank;
            byte_t eram_bank;
            byte_t ir_mode;
        } huc1;

        struct {
            byte_t rom_bank;
            byte_t eram_bank;
            byte_t mode;
        } huc3;
    };
} mbc_t;

void mbc_write_registers(emulator_t *emu, word_t address, byte_t data);

byte_t mbc_read_eram(emulator_t *emu, word_t address);

void mbc_write_eram(emulator_t *emu, word_t address, byte_t data);

void rtc_step(emulator_t *emu);
