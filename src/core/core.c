#include "core.h"
#include "gb/gb.h"
#include "gba/gba.h"

typedef void *(*init_func_t)(const uint8_t *rom, size_t rom_size, gbmulator_options_t *opts);
typedef void (*quit_func_t)(void *impl);
typedef void (*step_func_t)(void *impl);
typedef uint8_t *(*get_save_func_t)(void *impl, size_t *save_length);
typedef bool (*load_save_func_t)(void *impl, uint8_t *save_data, size_t save_length);
typedef uint8_t *(*get_savestate_func_t)(void *impl, size_t *length, bool is_compressed);
typedef bool (*load_savestate_func_t)(void *impl, uint8_t *data, size_t length);
typedef char *(*get_rom_title_func_t)(void *impl);
typedef void (*print_status_func_t)(void *impl);
typedef void (*set_joypad_state_func_t)(void *impl, uint16_t state);

struct gbmulator_t {
    gbmulator_mode_t mode;
    void *impl;

    init_func_t init;
    quit_func_t quit;
    step_func_t step;
    get_save_func_t get_save;
    load_save_func_t load_save;
    get_savestate_func_t get_savestate;
    load_savestate_func_t load_savestate;
    get_rom_title_func_t get_rom_title;
    print_status_func_t print_status;
    set_joypad_state_func_t set_joypad_state;
};

gbmulator_t *gbmulator_init(const uint8_t *rom, size_t rom_size, gbmulator_options_t *opts) {
    gbmulator_t *emu = xcalloc(1, sizeof(*emu));
    emu->mode = opts->mode;

    switch (opts->mode) {
    case GBMULATOR_MODE_GB:
    case GBMULATOR_MODE_GBC:
        emu->init = (init_func_t) gb_init;
        emu->quit = (quit_func_t) gb_quit;
        emu->step = (step_func_t) gb_step;
        emu->get_save = (get_save_func_t) gb_get_save;
        emu->load_save = (load_save_func_t) gb_load_save;
        emu->get_savestate = (get_savestate_func_t) gb_get_savestate;
        emu->load_savestate = (load_savestate_func_t) gb_load_savestate;
        emu->get_rom_title = (get_rom_title_func_t) gb_get_rom_title;
        emu->print_status = (print_status_func_t) gb_print_status;
        emu->set_joypad_state = (set_joypad_state_func_t) gb_set_joypad_state;
        break;
    case GBMULATOR_MODE_GBA:
        emu->init = (init_func_t) gba_init;
        emu->quit = (quit_func_t) gba_quit;
        emu->step = (step_func_t) gba_step;
        emu->get_save = (get_save_func_t) gba_get_save;
        emu->load_save = (load_save_func_t) gba_load_save;
        emu->get_savestate = (get_savestate_func_t) gba_get_savestate;
        emu->load_savestate = (load_savestate_func_t) gba_load_savestate;
        emu->get_rom_title = (get_rom_title_func_t) gba_get_rom_title;
        emu->print_status = (print_status_func_t) gba_print_status;
        emu->set_joypad_state = (set_joypad_state_func_t) gba_set_joypad_state;
        break;
    default:
        break;
    }

    emu->impl = emu->init(rom, rom_size, opts);

    if (!emu->impl) {
        free(emu);
        return NULL;
    }

    return emu;
}

void gbmulator_quit(gbmulator_t *emu) {
    emu->quit(emu->impl);
}

void gbmulator_step(gbmulator_t *emu) {
    emu->step(emu->impl);
}

void gbmulator_run_steps(gbmulator_t *emu, uint64_t steps_limit) {
    for (uint64_t steps_count = 0; steps_count < steps_limit; steps_count++)
        emu->step(emu->impl);
}

void gbmulator_run_frames(gbmulator_t *emu, uint64_t frames_limit) {
    gbmulator_run_steps(emu, frames_limit * GB_CPU_STEPS_PER_FRAME);
}

uint8_t *gbmulator_get_save(gbmulator_t *emu, size_t *save_length) {
    return emu->get_save(emu->impl, save_length);
}

bool gbmulator_load_save(gbmulator_t *emu, uint8_t *save_data, size_t save_length) {
    return emu->load_save(emu->impl, save_data, save_length);
}

uint8_t *gbmulator_get_savestate(gbmulator_t *emu, size_t *length, bool is_compressed) {
    return emu->get_savestate(emu->impl, length, is_compressed);
}

bool gbmulator_load_savestate(gbmulator_t *emu, uint8_t *data, size_t length) {
    return emu->load_savestate(emu->impl, data, length);
}

char *gbmulator_get_rom_title(gbmulator_t *emu) {
    return emu->get_rom_title(emu->impl);
}

void gbmulator_print_status(gbmulator_t *emu) {
    emu->print_status(emu->impl);
}

void gbmulator_set_joypad_state(gbmulator_t *emu, uint16_t state) {
    emu->set_joypad_state(emu->impl, state);
}
