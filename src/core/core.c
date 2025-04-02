#include "core.h"
#include "gb/gb.h"
#include "gba/gba.h"

// TODO use function pointers set in init instead of switches everywhere?

gbmulator_t *gbmulator_init(const uint8_t *rom, size_t rom_size, gbmulator_options_t *opts) {
    void *impl = NULL;

    switch (opts->mode) {
    case GBMULATOR_MODE_GB:
    case GBMULATOR_MODE_GBC:
        impl = gb_init(rom, rom_size, opts);
        break;
    case GBMULATOR_MODE_GBA:
        impl = gba_init(rom, rom_size, opts);
        break;
    default:
        return NULL;
    }

    if (!impl)
        return NULL;

    gbmulator_t *emu = xcalloc(1, sizeof(*emu));
    emu->mode = opts->mode;
    emu->impl = impl;

    return emu;
}

void gbmulator_quit(gbmulator_t *emu) {
    switch (emu->mode) {
    case GBMULATOR_MODE_GB:
    case GBMULATOR_MODE_GBC:
        gb_quit(emu->impl);
        break;
    case GBMULATOR_MODE_GBA:
        gba_quit(emu->impl);
        break;
    default:
        break;
    }
}

void gbmulator_step(gbmulator_t *emu) {
    switch (emu->mode) {
    case GBMULATOR_MODE_GB:
    case GBMULATOR_MODE_GBC:
        gb_step(emu->impl);
        break;
    case GBMULATOR_MODE_GBA:
        gba_step(emu->impl);
        break;
    default:
        break;
    }
}

void gbmulator_run_steps(gbmulator_t *emu, uint64_t steps_limit) {
    switch (emu->mode) {
    case GBMULATOR_MODE_GB:
    case GBMULATOR_MODE_GBC:
        for (uint64_t steps_count = 0; steps_count < steps_limit; steps_count++)
            gb_step(emu->impl);
        break;
    case GBMULATOR_MODE_GBA:
        for (uint64_t steps_count = 0; steps_count < steps_limit; steps_count++)
            gba_step(emu->impl);
        break;
    default:
        break;
    }
}

void gbmulator_run_frames(gbmulator_t *emu, uint64_t frames_limit) {
    gbmulator_run_steps(emu, frames_limit * GB_CPU_STEPS_PER_FRAME);
}

uint8_t *gbmulator_get_save(gbmulator_t *emu, size_t *save_length) {
    switch (emu->mode) {
    case GBMULATOR_MODE_GB:
    case GBMULATOR_MODE_GBC:
        return gb_get_save(emu->impl, save_length);
    case GBMULATOR_MODE_GBA:
        todo("gba_get_save");
        // return gba_get_save(emu->impl, save_length);
    default:
        return NULL;
    }
}

bool gbmulator_load_save(gbmulator_t *emu, uint8_t *save_data, size_t save_length) {
    switch (emu->mode) {
    case GBMULATOR_MODE_GB:
    case GBMULATOR_MODE_GBC:
        return gb_load_save(emu->impl, save_data, save_length);
    case GBMULATOR_MODE_GBA:
        todo("gba_load_save");
        // return gba_load_save(emu->impl, save_data, save_length);
    default:
        return NULL;
    }
}

uint8_t *gbmulator_get_savestate(gbmulator_t *emu, size_t *length, bool is_compressed) {
    switch (emu->mode) {
    case GBMULATOR_MODE_GB:
    case GBMULATOR_MODE_GBC:
        return gb_get_savestate(emu->impl, length, is_compressed);
    case GBMULATOR_MODE_GBA:
        todo("gb_get_savestate");
        // return gba_get_savestate(emu->impl, length, is_compressed);
    default:
        return NULL;
    }
}

bool gbmulator_load_savestate(gbmulator_t *emu, uint8_t *data, size_t length) {
    switch (emu->mode) {
    case GBMULATOR_MODE_GB:
    case GBMULATOR_MODE_GBC:
        return gb_load_savestate(emu->impl, data, length);
    case GBMULATOR_MODE_GBA:
        todo("gba_load_savestate");
        // return gba_load_savestate(emu->impl, data, length);
    default:
        return NULL;
    }
}

char *gbmulator_get_rom_title(gbmulator_t *emu) {
    switch (emu->mode) {
    case GBMULATOR_MODE_GB:
    case GBMULATOR_MODE_GBC:
        return gb_get_rom_title(emu->impl);
    case GBMULATOR_MODE_GBA:
        return gba_get_rom_title(emu->impl);
    default:
        return NULL;
    }
}

void gbmulator_print_status(gbmulator_t *emu) {
    switch (emu->mode) {
    case GBMULATOR_MODE_GB:
    case GBMULATOR_MODE_GBC:
        gb_print_status(emu->impl);
        break;
    case GBMULATOR_MODE_GBA:
        gba_print_status(emu->impl);
        break;
    default:
        break;
    }
}

void gbmulator_set_joypad_state(gbmulator_t *emu, uint16_t state) {
    switch (emu->mode) {
    case GBMULATOR_MODE_GB:
    case GBMULATOR_MODE_GBC:
        gb_set_joypad_state(emu->impl, state & 0x00FF);
        break;
    case GBMULATOR_MODE_GBA:
        gba_set_joypad_state(emu->impl, state & 0x03FF);
        break;
    default:
        break;
    }
}
