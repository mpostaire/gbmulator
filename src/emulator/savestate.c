#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>

#include "emulator.h"
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

typedef struct __attribute__((packed)) {
    save_header_t header;
    cpu_t cpu;
    byte_t mmu[sizeof(mmu_t) - offsetof(mmu_t, mem)];
    gbtimer_t timer;
} savestate_data_t;

int emulator_save_state(emulator_t *emu, const char *path) {
    make_parent_dirs(path);

    FILE *f = fopen(path, "wb");
    if (!f) {
        errnoprintf("opening %s", path);
        return 0;
    }

    save_header_t header = { .identifier = FORMAT_STRING };
    memcpy(header.rom_title, emu->rom_title, sizeof(header.rom_title));

    if (!fwrite(&header, sizeof(header), 1, f)) {
        eprintf("writing header to %s\n", path);
        fclose(f);
        return 0;
    }

    if (!fwrite(emu->cpu, sizeof(cpu_t), 1, f)) {
        eprintf("writing cpu to %s\n", path);
        fclose(f);
        return 0;
    }

    if (!fwrite(&emu->mmu->mem, sizeof(mmu_t) - offsetof(mmu_t, mem), 1, f)) {
        eprintf("writing mmu to %s\n", path);
        fclose(f);
        return 0;
    }

    if (!fwrite(emu->timer, sizeof(gbtimer_t), 1, f)) {
        eprintf("timer to %s\n", path);
        fclose(f);
        return 0;
    }

    fclose(f);
    return 1;
}

int emulator_load_state(emulator_t *emu, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        errnoprintf("opening %s", path);
        return 0;
    }

    save_header_t header;
    if (!fread(&header, sizeof(header), 1, f)) {
        errnoprintf("reading header from %s", path);
        fclose(f);
        return 0;
    }
    if (strncmp(header.identifier, FORMAT_STRING, sizeof(FORMAT_STRING))) {
        eprintf("invalid file format %s", path);
        fclose(f);
        return 0;
    }
    if (strncmp(header.rom_title, emu->rom_title, sizeof(header.rom_title))) {
        eprintf("rom title mismatch (expected: [%.16s]; got: [%.16s])\n", emu->rom_title, header.rom_title);
        fclose(f);
        return 0;
    }

    savestate_data_t *savestate = xmalloc(sizeof(savestate_data_t));

    if (!fread(&savestate->cpu, sizeof(cpu_t), 1, f)) {
        errnoprintf("reading cpu from %s", path);
        fclose(f);
        return 0;
    }

    if (!fread(savestate->mmu, sizeof(savestate->mmu), 1, f)) {
        errnoprintf("reading mmu from %s", path);
        fclose(f);
        return 0;
    }

    if (!fread(&savestate->timer, sizeof(gbtimer_t), 1, f)) {
        errnoprintf("reading timer from %s", path);
        fclose(f);
        return 0;
    }

    fclose(f);

    memcpy(emu->cpu, &savestate->cpu, sizeof(cpu_t));

    memcpy(&emu->mmu->mem, &savestate->mmu, sizeof(mmu_t) - offsetof(mmu_t, mem));

    memcpy(emu->timer, &savestate->timer, sizeof(gbtimer_t));

    // resets apu's internal state to prevent glitchy audio if resuming from state without sound playing from state with sound playing
    float sound = emu->apu->global_sound_level;
    float speed = emu->apu->speed;
    void (*cb)(float *, int);
    cb = emu->apu->samples_ready_cb;
    apu_quit(emu);
    apu_init(emu, sound, speed, cb);

    free(savestate);
    return 1;
}

byte_t *emulator_get_state_data(emulator_t *emu, size_t *length) {
    savestate_data_t *savestate = xmalloc(sizeof(savestate_data_t));

    memcpy(savestate->header.identifier, FORMAT_STRING, sizeof(savestate->header.identifier));

    memcpy(savestate->header.rom_title, emu->rom_title, sizeof(savestate->header.rom_title));

    memcpy(&savestate->cpu, emu->cpu, sizeof(cpu_t));

    memcpy(&savestate->mmu, &emu->mmu->mem, sizeof(mmu_t) - offsetof(mmu_t, mem));

    memcpy(&savestate->timer, emu->timer, sizeof(gbtimer_t));

    *length = sizeof(savestate_data_t);
    return (byte_t *) savestate;
}

int emulator_load_state_data(emulator_t *emu, const byte_t *data, size_t length) {
    if (length != sizeof(savestate_data_t)) {
        eprintf("invalid data length\n");
        return 0;
    }

    savestate_data_t *savestate = (savestate_data_t *) data;

    if (strncmp(savestate->header.identifier, FORMAT_STRING, sizeof(FORMAT_STRING))) {
        eprintf("invalid format\n");
        return 0;
    }
    if (strncmp(savestate->header.rom_title, emu->rom_title, sizeof(savestate->header.rom_title))) {
        eprintf("rom title mismatch (expected: [%.16s]; got: [%.16s])\n", emu->rom_title, savestate->header.rom_title);
        return 0;
    }

    memcpy(emu->cpu, &savestate->cpu, sizeof(cpu_t));

    memcpy(&emu->mmu->mem, &savestate->mmu, sizeof(mmu_t) - offsetof(mmu_t, mem));

    memcpy(emu->timer, &savestate->timer, sizeof(gbtimer_t));

    // resets apu's internal state to prevent glitchy audio if resuming from state without sound playing from state with sound playing
    float sound = emu->apu->global_sound_level;
    float speed = emu->apu->speed;
    void (*cb)(float *, int);
    cb = emu->apu->samples_ready_cb;
    apu_quit(emu);
    apu_init(emu, sound, speed, cb);

    return 1;
}
