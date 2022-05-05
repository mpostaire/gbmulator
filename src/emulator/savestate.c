#include <string.h>
#include <errno.h>

#include "emulator.h"
#include "utils.h"
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
        errnoprintf("opening %s", path);
        return 0;
    }

    save_header_t header = { .identifier = FORMAT_STRING };
    memcpy(header.rom_title, mmu.rom_title, sizeof(header.rom_title));

    if (!fwrite(&header, sizeof(header), 1, f)) {
        eprintf("writing header to %s\n", path);
        fclose(f);
        return 0;
    }

    if (!fwrite(&cpu, sizeof(cpu), 1, f)) {
        eprintf("writing cpu to %s\n", path);
        fclose(f);
        return 0;
    }

    if (!fwrite(&mmu.mem, sizeof(mmu) - offsetof(mmu_t, mem), 1, f)) {
        eprintf("writing mmu to %s\n", path);
        fclose(f);
        return 0;
    }

    if (!fwrite(&timer, sizeof(timer), 1, f)) {
        eprintf("timer to %s\n", path);
        fclose(f);
        return 0;
    }

    fclose(f);
    return 1;
}

int emulator_load_state(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        errnoprintf("opening %s", path);
        return 0;
    }

    save_header_t header;
    if (!fread(&header, sizeof(header), 1, f)) {
        eprintf("reading header from %s\n", path);
        fclose(f);
        return 0;
    }
    if (strncmp(header.identifier, FORMAT_STRING, sizeof(FORMAT_STRING))) {
        eprintf("invalid file format %s\n", path);
        fclose(f);
        return 0;
    }

    if (!fread(&cpu, sizeof(cpu), 1, f)) {
        eprintf("reading cpu from %s\n", path);
        fclose(f);
        return 0;
    }

    if (!fread(&mmu.mem, sizeof(mmu) - offsetof(mmu_t, mem), 1, f)) {
        eprintf("reading mmu from %s\n", path);
        fclose(f);
        return 0;
    }

    // resets apu's internal state to prevent glitchy audio if resuming from state without sound playing from state with sound playing
    apu_init(apu.global_sound_level, apu.sampling_freq_multiplier, apu.samples_ready_cb);

    if (!fread(&timer, sizeof(timer), 1, f)) {
        eprintf("reading timer from %s\n", path);
        fclose(f);
        return 0;
    }

    fclose(f);
    return 1;
}
