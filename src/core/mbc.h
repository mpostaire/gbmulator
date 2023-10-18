#pragma once

#include "types.h"

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
} gb_mbc_type_t;

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

            struct {
                byte_t latched_s;
                byte_t latched_m;
                byte_t latched_h;
                byte_t latched_dl;
                byte_t latched_dh;

                byte_t s;
                byte_t m;
                byte_t h;
                byte_t dl;
                byte_t dh;

                byte_t enabled;
                byte_t reg; // rtc register
                byte_t latch;
                uint32_t rtc_cycles;
            } rtc;
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
            byte_t ir_mode; // 1 if huc1 eram is in IR mode
            byte_t ir_led;
            byte_t rom_bank;
            byte_t eram_bank;
        } huc1;

        struct {
            byte_t rom_bank;
            byte_t eram_bank;
            byte_t cam_regs_enabled;
            uint32_t cam_cycles_left;
            byte_t regs[0x36];
        } camera;
    };
} gb_mbc_t;

void mbc_write_registers(gb_t *gb, word_t address, byte_t data);

byte_t mbc_read_eram(gb_t *gb, word_t address);

void mbc_write_eram(gb_t *gb, word_t address, byte_t data);

void rtc_step(gb_t *gb);

#define MBC_COMMON_MEMBERS \
    X(type)                \
    X(eram_enabled)

#define MBC1_MEMBERS \
    X(mbc1.mode)     \
    X(mbc1.bank_lo)  \
    X(mbc1.bank_hi)

#define MBC2_MEMBERS \
    X(mbc2.rom_bank)

#define RTC_MEMBERS        \
    X(mbc3.rtc.latched_s)  \
    X(mbc3.rtc.latched_m)  \
    X(mbc3.rtc.latched_h)  \
    X(mbc3.rtc.latched_dl) \
    X(mbc3.rtc.latched_dh) \
    X(mbc3.rtc.s)          \
    X(mbc3.rtc.m)          \
    X(mbc3.rtc.h)          \
    X(mbc3.rtc.dl)         \
    X(mbc3.rtc.dh)         \
    X(mbc3.rtc.enabled)    \
    X(mbc3.rtc.reg)        \
    X(mbc3.rtc.latch)      \
    X(mbc3.rtc.rtc_cycles)

#define MBC3_MEMBERS   \
    X(mbc3.rtc_mapped) \
    X(mbc3.rom_bank)   \
    X(mbc3.eram_bank)  \
    RTC_MEMBERS

#define MBC5_MEMBERS    \
    X(mbc5.rom_bank_lo) \
    X(mbc5.rom_bank_hi) \
    X(mbc5.eram_bank)

#define MBC7_MEMBERS                          \
    X(mbc7.eram_enabled2)                     \
    X(mbc7.rom_bank)                          \
    X(mbc7.accelerometer.latched_x)           \
    X(mbc7.accelerometer.latched_y)           \
    X(mbc7.accelerometer.latch_ready)         \
    X(mbc7.eeprom.data)                       \
    X(mbc7.eeprom.pins)                       \
    X(mbc7.eeprom.command)                    \
    X(mbc7.eeprom.command_arg_remaining_bits) \
    X(mbc7.eeprom.output_bits)                \
    X(mbc7.eeprom.write_enabled)

#define HuC1_MEMBERS \
    X(huc1.ir_mode)  \
    X(huc1.ir_led)  \
    X(huc1.rom_bank) \
    X(huc1.eram_bank)

// TODO serialize CAMERA MBC

#define MBC_SERIALIZED_MEMBERS \
    MBC_COMMON_MEMBERS         \
    switch (tmp->type) {       \
    case MBC1:                 \
    case MBC1M:                \
        MBC1_MEMBERS           \
        break;                 \
    case MBC2:                 \
        MBC2_MEMBERS           \
        break;                 \
    case MBC3:                 \
    case MBC30:                \
        MBC3_MEMBERS           \
        break;                 \
    case MBC5:                 \
        MBC5_MEMBERS           \
        break;                 \
    case MBC7:                 \
        MBC7_MEMBERS           \
        break;                 \
    case HuC1:                 \
        HuC1_MEMBERS break;    \
    }
