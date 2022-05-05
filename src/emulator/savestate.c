#include <string.h>

#include "emulator.h"
#include "cpu.h"
#include "boot.h"
#include "timer.h"

#define FORMAT_STRING EMULATOR_NAME"-sav"

typedef struct __attribute__((packed)) {
    char identifier[sizeof(FORMAT_STRING)];
    char rom_title[16];
} save_header_t;

int emulator_save_state(const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        perror("ERROR: emulator_save_state: opening the savestate file");
        return 0;
    }

    save_header_t header = { .identifier = FORMAT_STRING };
    memcpy(header.rom_title, mmu.rom_title, sizeof(header.rom_title));

    if (!fwrite(&header, sizeof(header), 1, f)) {
        printf("ERROR: emulator_save_state: writing header to savestate file\n");
        fclose(f);
        return 0;
    }

    if (!fwrite(&cpu, sizeof(cpu), 1, f)) {
        printf("ERROR: emulator_save_state: writing cpu to savestate file\n");
        fclose(f);
        return 0;
    }

    if (!fwrite(&mmu.mem, sizeof(mmu) - offsetof(mmu_t, mem), 1, f)) {
        printf("ERROR: emulator_save_state: writing mmu to savestate file\n");
        fclose(f);
        return 0;
    }

    if (!fwrite(&timer, sizeof(timer), 1, f)) {
        printf("ERROR: emulator_save_state: writing timer to file\n");
        fclose(f);
        return 0;
    }

    fclose(f);
    return 1;
}

int emulator_load_state(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror("ERROR: emulator_load_state: opening the savestate file");
        return 0;
    }

    save_header_t header;
    if (!fread(&header, sizeof(header), 1, f)) {
        printf("ERROR: emulator_load_state: reading header from savestate file\n");
        fclose(f);
        return 0;
    }
    if (strncmp(header.identifier, FORMAT_STRING, sizeof(FORMAT_STRING))) {
        printf("ERROR: emulator_load_state: invalid file format\n");
        fclose(f);
        return 0;
    }

    if (!fread(&cpu, sizeof(cpu), 1, f)) {
        printf("ERROR: emulator_load_state: reading cpu from savestate file\n");
        fclose(f);
        return 0;
    }

    if (!fread(&mmu.mem, sizeof(mmu) - offsetof(mmu_t, mem), 1, f)) {
        printf("ERROR: emulator_load_state: reading mmu from savestate file\n");
        fclose(f);
        return 0;
    }

    // resets apu's internal state to prevent glitchy audio if resuming from state without sound playing from state with sound playing
    apu_init(apu.global_sound_level, apu.sampling_freq_multiplier, apu.samples_ready_cb);

    if (!fread(&timer, sizeof(timer), 1, f)) {
        printf("ERROR: emulator_load_state: reading timer from savestate file\n");
        fclose(f);
        return 0;
    }

    fclose(f);
    return 1;
}
