#include <string.h>
#include <errno.h>

#include "emulator.h"
#include "utils.h"
#include "cpu.h"
#include "boot.h"
#include "timer.h"
#include "mmu.h"
#include "cpu.h"
#include "apu.h"

#define FORMAT_STRING EMULATOR_NAME"-sav"

typedef struct __attribute__((packed)) {
    char identifier[sizeof(FORMAT_STRING)];
    char rom_title[16];
} save_header_t;

int emulator_save_state(const char *path) {
    make_parent_dirs(path);

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
    if (strncmp(header.rom_title, mmu.rom_title, sizeof(header.rom_title))) {
        eprintf("rom title mismatch (expected: [%.16s]; got: [%.16s])\n", mmu.rom_title, header.rom_title);
        fclose(f);
        return 0;
    }

    if (!fread(&mmu.mem, sizeof(mmu) - offsetof(mmu_t, mem), 1, f)) {
        eprintf("reading mmu from %s\n", path);
        fclose(f);
        return 0;
    }

    // resets apu's internal state to prevent glitchy audio if resuming from state without sound playing from state with sound playing
    apu_init(apu.global_sound_level, apu.speed, apu.samples_ready_cb);

    if (!fread(&timer, sizeof(timer), 1, f)) {
        eprintf("reading timer from %s\n", path);
        fclose(f);
        return 0;
    }

    fclose(f);
    return 1;
}

typedef struct __attribute__((packed)) {
    save_header_t header;
    cpu_t cpu;
    byte_t mmu[sizeof(mmu) - offsetof(mmu_t, mem)];
    timer_t timer;
} savestate_data_t;

byte_t *emulator_get_state_data(size_t *length) {
    savestate_data_t *savestate = xmalloc(sizeof(savestate_data_t));

    memcpy(&savestate->header.identifier, FORMAT_STRING, sizeof(savestate->header.identifier));

    memcpy(&savestate->header.rom_title, mmu.rom_title, sizeof(savestate->header.rom_title));
    for (int i = 0; i < 16; i++)
        if (savestate->header.rom_title[i] == '\0')
            savestate->header.rom_title[i] = ' ';

    memcpy(&savestate->cpu, &cpu, sizeof(cpu));

    memcpy(&savestate->mmu, &mmu.mem, sizeof(mmu) - offsetof(mmu_t, mem));

    memcpy(&savestate->timer, &timer, sizeof(timer));

    *length = sizeof(savestate_data_t);
    return (byte_t *) savestate;
}

int emulator_load_state_data(const byte_t *data, size_t length) {
    if (length != sizeof(savestate_data_t)) {
        eprintf("invalid data length\n");
        return 0;
    }

    savestate_data_t *savestate = (savestate_data_t *) data;

    if (strncmp(savestate->header.identifier, FORMAT_STRING, sizeof(FORMAT_STRING))) {
        eprintf("invalid format\n");
        return 0;
    }
    if (strncmp(savestate->header.rom_title, mmu.rom_title, sizeof(savestate->header.rom_title))) {
        eprintf("rom title mismatch (expected: [%.16s]; got: [%.16s])\n", mmu.rom_title, savestate->header.rom_title);
        return 0;
    }

    memcpy(&cpu, &savestate->cpu, sizeof(cpu));

    memcpy(&mmu.mem, &savestate->mmu, sizeof(mmu) - offsetof(mmu_t, mem));

    // resets apu's internal state to prevent glitchy audio if resuming from state without sound playing from state with sound playing
    apu_init(apu.global_sound_level, apu.speed, apu.samples_ready_cb);

    memcpy(&timer, &savestate->timer, sizeof(timer));

    return 1;
}
