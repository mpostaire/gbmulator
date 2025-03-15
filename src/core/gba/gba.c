#include <stdio.h>
#include <stdlib.h>

#include "gba_priv.h"

void gba_step(gba_t *gba) {
    gba_cpu_step(gba);
}

gba_t *gba_init(const uint8_t *rom, size_t rom_size, gba_config_t *config) {
    gba_t *gba = xcalloc(1, sizeof(*gba));

    gba->bus = gba_bus_init(rom, rom_size);
    if (!gba->bus) {
        free(gba);
        return NULL;
    }

    gba->cpu = gba_cpu_init();

    return gba;
}

void gba_quit(gba_t *gba) {
    gba_cpu_quit(gba->cpu);
    gba_bus_quit(gba->bus);
    free(gba);
}

void gba_print_status(gba_t *gba) {
    // TODO
}
