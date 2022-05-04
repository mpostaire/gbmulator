#include <stdint.h>
#include <string.h>

#include "emulator.h"
#include "boot.h"

#define BESS_VERSION_MAJOR 1
#define BESS_VERSION_MINOR 1

// TODO code cleaning

// https://github.com/LIJI32/SameBoy/blob/master/BESS.md

typedef struct __attribute__((packed)) {
    char identifier[4];
    uint32_t length;
} BESS_header;

typedef struct __attribute__((packed)) {
    uint32_t first_block_offset;
    char identifier[4];
} BESS_footer;

typedef struct __attribute__((packed)) {
    char name_version[sizeof(EMULATOR_NAME" "EMULATOR_VERSION) - 1];
} BESS_NAME_block;

typedef struct __attribute__((packed)) {
    char rom_title[0x10];
    word_t rom_globlal_checksum;
} BESS_INFO_block;

typedef struct __attribute__((packed)) {
    word_t major;
    word_t minor;
    char model[4];

    word_t pc;
    word_t af;
    word_t bc;
    word_t de;
    word_t hl;
    word_t sp;
    byte_t ime;
    byte_t ie;
    byte_t execution_state;
    byte_t _reserved;
    byte_t io_registers[128];

    uint32_t ram_size;
    uint32_t ram_offset;
    uint32_t vram_size;
    uint32_t vram_offset;
    uint32_t mbc_ram_size;
    uint32_t mbc_ram_offset;
    uint32_t oam_size;
    uint32_t oam_offset;
    uint32_t hram_size;
    uint32_t hram_offset;
    uint32_t background_palettes_size;
    uint32_t background_palettes_offset;
    uint32_t object_palettes_size;
    uint32_t object_palettes_offset;
} BESS_CORE_block;

typedef struct __attribute__((packed)) {
    word_t address;
    byte_t value;
} BESS_MBC_pair;

typedef struct __attribute__((packed)) {
    BESS_MBC_pair *pairs;
} BESS_MBC_block;

typedef struct __attribute__((packed)) {
    byte_t s;
    byte_t _padding0[3];
    byte_t m;
    byte_t _padding1[3];
    byte_t h;
    byte_t _padding2[3];
    byte_t d;
    byte_t _padding3[3];
    byte_t high_overflow_running; // TODO what is that?????

    byte_t _padding4[3];
    byte_t latched_s;
    byte_t _padding5[3];
    byte_t latched_m;
    byte_t _padding6[3];
    byte_t latched_h;
    byte_t _padding7[3];
    byte_t latched_d;
    byte_t _padding8[3];
    byte_t latched_high_overflow_running; // TODO what is that?????
    byte_t _padding9[3];

    uint64_t timestamp;
} BESS_RTC_block;

int emulator_save_state(const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        perror("ERROR: emulator_save_state: opening the savestate file");
        exit(EXIT_FAILURE);
    }
    // TODO fail on any write error

    // NAME block
    BESS_header name_header = { "NAME", sizeof(EMULATOR_NAME" "EMULATOR_VERSION) - 1 };
    BESS_NAME_block name_block = { EMULATOR_NAME" "EMULATOR_VERSION };
    if (!fwrite(&name_header, sizeof(name_header), 1, f) ||
        !fwrite(&name_block, sizeof(name_block), 1, f))
        printf("ERROR: emulator_save_state: writing NAME block to savestate file\n");

    // INFO block
    BESS_header info_header = { "INFO", 0x12 };
    BESS_INFO_block info_block = { .rom_globlal_checksum = cartridge[0x014E] << 8 | cartridge[0x014F] };
    char *rom_title = mmu_get_rom_title();
    memcpy(info_block.rom_title, rom_title, strlen(rom_title));
    if (!fwrite(&info_header, sizeof(info_header), 1, f) ||
        !fwrite(&info_block, sizeof(info_block), 1, f))
        printf("ERROR: emulator_save_state: writing INFO block to savestate file\n");

    // MBC block
    uint32_t mbc_block_length = 0;
    BESS_header mbc_header;
    BESS_MBC_block mbc_block;
    switch (mmu.mbc) {
    case MBC1:
        mbc_block_length = 12;
        mbc_header = (BESS_header) { "MBC ", mbc_block_length };
        mbc_block = (BESS_MBC_block) {
            .pairs = (BESS_MBC_pair[4]) {
                { 0x0000, mmu.eram_enabled ? 0x0A : 0x00 },
                { 0x2000, mmu.current_rom_bank },
                { 0x4000, mmu.mbc1_mode ? mmu.current_eram_bank : mmu.current_rom_bank },
                { 0x6000, mmu.mbc1_mode }
            }
        };
        break;
    case MBC2:
        mbc_header = (BESS_header) { "MBC ", mbc_block_length };
        break;
    case MBC3:
        mbc_header = (BESS_header) { "MBC ", mbc_block_length };
        break;
    default:
        printf("ERROR: emulator_save_state: MBC > 3 not supported\n");
        return 0;
    }
    
    // CORE block
    BESS_header core_header = { "CORE", 0xD0 };
    BESS_CORE_block core_block = {
        .major = BESS_VERSION_MAJOR,
        .minor = BESS_VERSION_MINOR,
        .model = "GD  ", // TODO change this when support for CGB is added
        .pc = registers.pc,
        .af = registers.af,
        .bc = registers.bc,
        .de = registers.de,
        .hl = registers.hl,
        .sp = registers.sp,
        .ime = cpu_ime,
        .ie = mem[IE],
        .execution_state = cpu_halt, // TODO STOP instruction is not implemented yet so the only possible values are cpu_halt
        .ram_size = ECHO - WRAM_BANK0,
        .vram_size = ERAM - VRAM,
        .mbc_ram_size = mmu.mbc == MBC0 ? 0 : sizeof(eram),
        .oam_size = UNUSABLE - OAM,
        .hram_size = IE - HRAM,
        .background_palettes_size = OBP0 - BGP,
        .object_palettes_size = WY - OBP0,
    };
    core_block.ram_offset = sizeof(BESS_NAME_block) + sizeof(BESS_INFO_block) + sizeof(BESS_CORE_block) + (4 * sizeof(BESS_header));
    if (mbc_block_length)
        core_block.ram_offset += mbc_block_length + sizeof(BESS_header);
    core_block.vram_offset = core_block.ram_offset + core_block.ram_size;
    core_block.mbc_ram_offset = core_block.vram_offset + core_block.vram_size;
    core_block.oam_offset = core_block.mbc_ram_offset + core_block.mbc_ram_size;
    core_block.hram_offset = core_block.oam_offset + core_block.oam_size;
    core_block.background_palettes_offset = core_block.hram_offset + core_block.hram_size;
    core_block.object_palettes_offset = core_block.background_palettes_offset + core_block.background_palettes_size;
    memcpy(&core_block.io_registers, &mem[IO], sizeof(core_block.io_registers));
    if (!fwrite(&core_header, sizeof(core_header), 1, f) ||
        !fwrite(&core_block, sizeof(core_block), 1, f)) {
        printf("ERROR: emulator_save_state: writing CORE block to savestate file\n");
        fclose(f);
        return 0;
    }

    // write MBC block
    if (mbc_block_length) {
        if (!fwrite(&mbc_header, sizeof(mbc_header), 1, f) ||
            !fwrite(&mbc_block, mbc_block_length, 1, f)) {
            printf("ERROR: emulator_save_state: writing MBC block to savestate file\n");
            fclose(f);
            return 0;
        }
    }

    // this block is only if using mbc3 with an rtc
    // BESS_header rtc_header = { "RTC ", 0x30 };
    // BESS_RTC_block rtc_block = {
    // };

    BESS_header end_block = {
        .identifier = "END ",
        .length = 0
    };
    if (!fwrite(&end_block, sizeof(end_block), 1, f)) {
        printf("ERROR: emulator_save_state: writing END block to savestate file\n");
        fclose(f);
        return 0;
    }

    if (!fwrite(&mem[WRAM_BANK0], core_block.ram_size, 1, f)) {
        printf("ERROR: emulator_save_state: writing ram to savestate file\n");
        fclose(f);
        return 0;
    }
    if (!fwrite(&mem[VRAM], core_block.vram_size, 1, f)) {
        printf("ERROR: emulator_save_state: writing vram to savestate file\n");
        fclose(f);
        return 0;
    }
    if (mmu.mbc && !fwrite(&mem[ERAM], core_block.mbc_ram_size, 1, f)) {
        printf("ERROR: emulator_save_state: writing mbc ram to savestate file\n");
        fclose(f);
        return 0;
    }
    if (!fwrite(&mem[OAM], core_block.oam_size, 1, f)) {
        printf("ERROR: emulator_save_state: writing oam to savestate file\n");
        fclose(f);
        return 0;
    }
    if (!fwrite(&mem[HRAM], core_block.hram_size, 1, f)) {
        printf("ERROR: emulator_save_state: writing hram to savestate file\n");
        fclose(f);
        return 0;
    }
    if (!fwrite(&mem[BGP], core_block.background_palettes_size, 1, f)) {
        printf("ERROR: emulator_save_state: writing bgp to savestate file\n");
        fclose(f);
        return 0;
    }
    if (!fwrite(&mem[OBP0], core_block.object_palettes_size, 1, f)) {
        printf("ERROR: emulator_save_state: writing obp to savestate file\n");
        fclose(f);
        return 0;
    }

    BESS_footer footer = {
        .first_block_offset = 0,
        .identifier = "BESS"
    };
    if (!fwrite(&footer, sizeof(footer), 1, f)) {
        printf("ERROR: emulator_save_state: writing footer to savestate file\n");
        fclose(f);
        return 0;
    }

    fclose(f);
    return 1;
}

int emulator_load_state(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        // TODO do nothing? (maybe print message) there is no save state or an error? use errno to differentiate?
        return 0;
        // perror("ERROR: emulator_load_state: opening the savestate file");
        // exit(EXIT_FAILURE);
    }

    // parse footer first to quickly check if it's a valid format
    BESS_footer footer;
    if (fseek(f, -sizeof(footer), SEEK_END) == -1) {
        perror("ERROR: emulator_load_state");
        fclose(f);
        return 0;
    }
    if (!fread(&footer, sizeof(footer), 1, f)) {
        printf("ERROR: emulator_load_state: reading footer from savestate file\n");
        fclose(f);
        return 0;
    }
    if (strncmp(footer.identifier, "BESS", 4)) {
        printf("ERROR: emulator_load_state: invalid file format\n");
        fclose(f);
        return 0;
    }
    fseek(f, footer.first_block_offset, SEEK_SET);

    BESS_CORE_block core_block;
    BESS_MBC_block mbc_block;
    uint32_t mbc_block_length = 0;
    byte_t info_present = 0;
    byte_t core_present = 0;
    byte_t end_block_reached = 0;
    // TODO place all if contents to their own functions for readabiliy?
    while (!end_block_reached) {
        BESS_header header;
        if (!fread(&header, sizeof(header), 1, f)) {
            printf("ERROR: emulator_load_state: reached end-of-file before END block\n");
            // TODO no end of block reached --> should print error or ignore and attempt anyway?
            fclose(f);
            return 0;
        }

        if (!strncmp(header.identifier, "NAME", sizeof(header.identifier))) {
            if (info_present) {
                printf("ERROR: emulator_load_state: INFO block before NAME\n");
                fclose(f);
                return 0;
            }
            char name_version[header.length + 1];
            if (!fread(name_version, header.length, 1, f)) {
                printf("ERROR: emulator_load_state: reading NAME block from savestate file\n");
                fclose(f);
                return 0;
            }
            name_version[header.length] = '\0';
            // do nothing with this yet...
        } else if (!strncmp(header.identifier, "INFO", sizeof(header.identifier))) {
            BESS_INFO_block info_block;
            if (!fread(&info_block, header.length, 1, f)) {
                printf("ERROR: emulator_load_state: reading INFO block from savestate file\n");
                fclose(f);
                return 0;
            }
            int len = info_block.rom_title[15] != '\0' ? sizeof(info_block.rom_title) : strlen(info_block.rom_title);
            char *current_rom_title = mmu_get_rom_title();
            if (len != strlen(current_rom_title) || strncmp(info_block.rom_title, current_rom_title, len)) {
                printf("ERROR: emulator_load_state: INFO block rom title mismatch\n");
                fclose(f);
                return 0;
            }
            // rom_global_checksum not implemented
            info_present = 1;
        } else if (!strncmp(header.identifier, "CORE", sizeof(header.identifier))) {
            if (core_present) {
                printf("ERROR: emulator_load_state: duplicated CORE block\n");
                fclose(f);
                return 0;
            }
            if (!fread(&core_block, header.length, 1, f)) {
                printf("ERROR: emulator_load_state: reading CORE block from savestate file\n");
                fclose(f);
                return 0;
            }
            if (core_block.major != BESS_VERSION_MAJOR) {
                printf("ERROR: emulator_load_state: CORE block version major %d != %d\n", core_block.major, BESS_VERSION_MAJOR);
                fclose(f);
                return 0;
            }
            if (core_block.model[0] != 'G' && core_block.model[1] != 'D') {
                printf("ERROR: emulator_load_state: CORE block model %s != GD\n", core_block.model);
                fclose(f);
                return 0;
            }
            core_present = 1;
        } else if (!strncmp(header.identifier, "XOAM", sizeof(header.identifier))) {
            // not implemented
            fseek(f, header.length, SEEK_CUR);
        } else if (!strncmp(header.identifier, "MBC ", sizeof(header.identifier))) {
            if (!(header.length % sizeof(BESS_MBC_pair)) && mmu.mbc) {
                if (!fread(&mbc_block, header.length, 1, f)) {
                    printf("ERROR: emulator_load_state: reading MBC block from savestate file\n");
                    fclose(f);
                    return 0;
                }
                mbc_block_length = header.length;
            } else {
                printf("ERROR: emulator_load_state: invalid MBC block detected in %s\n - skipping", path);
                fseek(f, header.length, SEEK_CUR);
            }
        } else if (!strncmp(header.identifier, "RTC ", sizeof(header.identifier))) {
            // TODO error when not present but rom is using mbc3 + rtc
        } else if (!strncmp(header.identifier, "HUC3", sizeof(header.identifier))) {
            // not implemented
            printf("WARNING: emulator_load_state: ignore unsupported HUC3 block\n");
            fseek(f, header.length, SEEK_CUR);
        } else if (!strncmp(header.identifier, "SGB ", sizeof(header.identifier))) {
            // not implemented
            printf("WARNING: emulator_load_state: ignore unsupported HUC3 block\n");
            fseek(f, header.length, SEEK_CUR);
        } else if (!strncmp(header.identifier, "END ", sizeof(header.identifier))) {
            if (header.length != 0) {
                printf("ERROR: emulator_load_state: END block length != 0\n");
                fclose(f);
                return 0;
            }
            end_block_reached = 1;
        } else {
            printf("ERROR: emulator_load_state: invalid block detected (%*s) in %s\n - skipping", (int) sizeof(header.identifier), header.identifier, path);
            fseek(f, header.length, SEEK_CUR);
        }
    }

    if (!core_present) {
        printf("ERROR: emulator_load_state: reached end-of-file before CORE block\n");
        fclose(f);
        return 0;
    }

    registers.pc = core_block.pc;
    registers.af = core_block.af;
    registers.bc = core_block.bc;
    registers.de = core_block.de;
    registers.hl = core_block.hl;
    registers.sp = core_block.sp;

    cpu_ime = core_block.ime;
    mem[IE] = core_block.ie;
    cpu_halt = core_block.execution_state == 1;

    memcpy(&mem[IO], &core_block.io_registers, sizeof(core_block.io_registers));
    // resets apu's internal state to prevent glitchy audio if resuming from state without sound playing from state with sound playing
    apu_init();

    if (mbc_block_length) {
        switch (mmu.mbc) {
        case MBC1:
            s_word_t temp = 0;
            for (int i = 0; i < mbc_block_length / sizeof(BESS_MBC_pair); i++) {
                switch (mbc_block.pairs[i].address) {
                case 0x0000:
                    mmu.eram_enabled = mbc_block.pairs[i].value == 0x0A;
                    break;
                case 0x2000:
                    mmu.current_rom_bank = mbc_block.pairs[i].value;
                    break;
                case 0x4000:
                    temp = mbc_block.pairs[i].value;
                    break;
                case 0x6000:
                    mmu.mbc1_mode = mbc_block.pairs[i].value;
                    break;
                }
            }
            if (mmu.mbc1_mode)
                mmu.current_eram_bank = temp;
            else
                mmu.current_rom_bank = temp;
            break;
        case MBC2:
            /* code */
            break;
        case MBC3:
            /* code */
            break;
        default:
            printf("ERROR: emulator_load_state: MBC > 3 not supported\n");
            break;
        }
    }

    if (mem[BANK])
        memcpy(&mem, &cartridge, 0x100);
    else
        memcpy(&mem, &dmg_boot, sizeof(dmg_boot));

    char *buf = malloc(core_block.ram_size);
    fseek(f, core_block.ram_offset, SEEK_SET);
    if (!fread(buf, core_block.ram_size, 1, f)) {
        printf("ERROR: emulator_load_state: reading ram from savestate file\n");
        fclose(f);
        return 0;
    }
    memcpy(&mem[WRAM_BANK0], buf, core_block.ram_size);
    free(buf);

    buf = malloc(core_block.ram_size);
    fseek(f, core_block.vram_offset, SEEK_SET);
    if (!fread(buf, core_block.vram_size, 1, f)) {
        printf("ERROR: emulator_load_state: reading vram from savestate file\n");
        fclose(f);
        return 0;
    }
    memcpy(&mem[VRAM], buf, core_block.vram_size);
    free(buf);

    if (core_block.mbc_ram_size > 0) {
        buf = malloc(core_block.mbc_ram_size);
        fseek(f, core_block.mbc_ram_offset, SEEK_SET);
        if (!fread(buf, core_block.mbc_ram_size, 1, f)) {
            printf("ERROR: emulator_load_state: reading mbc ram from savestate file\n");
            fclose(f);
            return 0;
        }
        memcpy(&eram, buf, core_block.mbc_ram_size);
        free(buf);
    }

    buf = malloc(core_block.ram_size);
    fseek(f, core_block.oam_offset, SEEK_SET);
    if (!fread(buf, core_block.oam_size, 1, f)) {
        printf("ERROR: emulator_load_state: reading oam from savestate file\n");
        fclose(f);
        return 0;
    }
    memcpy(&mem[OAM], buf, core_block.oam_size);
    free(buf);

    buf = malloc(core_block.ram_size);
    fseek(f, core_block.hram_offset, SEEK_SET);
    if (!fread(buf, core_block.hram_size, 1, f)) {
        printf("ERROR: emulator_load_state: reading hram from savestate file\n");
        fclose(f);
        return 0;
    }
    memcpy(&mem[HRAM], buf, core_block.hram_size);
    free(buf);

    buf = malloc(core_block.ram_size);
    fseek(f, core_block.background_palettes_offset, SEEK_SET);
    if (!fread(buf, core_block.background_palettes_size, 1, f)) {
        printf("ERROR: emulator_load_state: reading bgp from savestate file\n");
        fclose(f);
        return 0;
    }
    memcpy(&mem[BGP], buf, core_block.background_palettes_size);
    free(buf);

    buf = malloc(core_block.ram_size);
    fseek(f, core_block.object_palettes_offset, SEEK_SET);
    if (!fread(buf, core_block.object_palettes_size, 1, f)) {
        printf("ERROR: emulator_load_state: reading obp from savestate file\n");
        fclose(f);
        return 0;
    }
    memcpy(&mem[OBP0], buf, core_block.object_palettes_size);
    free(buf);

    fclose(f);
    return 1;
}
