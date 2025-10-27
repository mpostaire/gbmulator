#include "core_priv.h"
#include "gb/gb.h"
#include "gba/gba.h"
#include "gbprinter/gbprinter.h"

#define N_REWIND_STATES           (8 * GB_FRAMES_PER_SECOND)
#define DEFAULT_APU_SAMPLING_RATE 44100

static bool set_funcs(gbmulator_t *emu, gbmulator_mode_t mode) {
    switch (mode) {
    case GBMULATOR_MODE_GB:
    case GBMULATOR_MODE_GBC:
        emu->init                = (init_func_t) gb_init;
        emu->quit                = (quit_func_t) gb_quit;
        emu->step                = (step_func_t) gb_step;
        emu->get_save            = (get_save_func_t) gb_get_save;
        emu->load_save           = (load_save_func_t) gb_load_save;
        emu->get_savestate       = (get_savestate_func_t) gb_get_savestate;
        emu->load_savestate      = (load_savestate_func_t) gb_load_savestate;
        emu->get_rom_title       = (get_rom_title_func_t) gb_get_rom_title;
        emu->print_status        = (print_status_func_t) gb_print_status;
        emu->get_joypad_state    = (get_joypad_state_func_t) gb_get_joypad_state;
        emu->set_joypad_state    = (set_joypad_state_func_t) gb_set_joypad_state;
        emu->get_rom             = (get_rom_func_t) gb_get_rom;
        emu->cable.shift_bit     = (cable_shift_bit_cb_t) gb_link_shift_bit;
        emu->cable.data_received = (cable_data_received_cb_t) gb_link_data_received;
        return true;
    case GBMULATOR_MODE_GBA:
        emu->init                = (init_func_t) gba_init;
        emu->quit                = (quit_func_t) gba_quit;
        emu->step                = (step_func_t) gba_step;
        emu->get_save            = (get_save_func_t) gba_get_save;
        emu->load_save           = (load_save_func_t) gba_load_save;
        emu->get_savestate       = (get_savestate_func_t) gba_get_savestate;
        emu->load_savestate      = (load_savestate_func_t) gba_load_savestate;
        emu->get_rom_title       = (get_rom_title_func_t) gba_get_rom_title;
        emu->print_status        = (print_status_func_t) gba_print_status;
        emu->get_joypad_state    = (get_joypad_state_func_t) gba_get_joypad_state;
        emu->set_joypad_state    = (set_joypad_state_func_t) gba_set_joypad_state;
        emu->get_rom             = (get_rom_func_t) gba_get_rom;
        emu->cable.shift_bit     = NULL;
        emu->cable.data_received = NULL;
        return true;
    case GBMULATOR_MODE_GBPRINTER:
        emu->init                = (init_func_t) gbprinter_init;
        emu->quit                = (quit_func_t) gbprinter_quit;
        emu->step                = (step_func_t) gbprinter_step;
        emu->get_save            = (get_save_func_t) gbprinter_get_image;
        emu->load_save           = NULL;
        emu->get_savestate       = NULL;
        emu->load_savestate      = NULL;
        emu->get_rom_title       = NULL;
        emu->print_status        = NULL;
        emu->get_joypad_state    = NULL;
        emu->set_joypad_state    = NULL;
        emu->get_rom             = NULL;
        emu->cable.shift_bit     = (cable_shift_bit_cb_t) gbprinter_link_shift_bit;
        emu->cable.data_received = (cable_data_received_cb_t) gbprinter_link_data_received;
        return true;
    default:
        return false;
    }
}

gbmulator_t *gbmulator_init(const gbmulator_options_t *opts) {
    if (!opts)
        return NULL;

    gbmulator_t *emu = xcalloc(1, sizeof(*emu));
    gbmulator_set_options(emu, opts);

    if (!set_funcs(emu, emu->opts.mode)) {
        free(emu);
        return NULL;
    }

    emu->impl = emu->init(emu);

    if (!emu->impl) {
        free(emu);
        return NULL;
    }

    emu->rewind_stack.states = xmalloc(N_REWIND_STATES * /*get_savestate_expected_len(emu)*/ 100000);
    emu->rewind_stack.head   = -1;

    return emu;
}

void gbmulator_quit(gbmulator_t *emu) {
    if (!emu)
        return;

    if (emu->rewind_stack.states)
        free(emu->rewind_stack.states);

    gbmulator_link_disconnect(emu, GBMULATOR_LINK_CABLE);
    gbmulator_link_disconnect(emu, GBMULATOR_LINK_IR);

    emu->quit(emu->impl);

    free(emu);
}

bool gbmulator_reset(gbmulator_t *emu, gbmulator_mode_t new_mode) {
    if (!emu)
        return false;

    size_t   rom_size;
    uint8_t *rom = emu->get_rom(emu->impl, &rom_size);

    if (rom_size == 0 || !rom)
        return false;

    uint8_t *rom_bak = xmalloc(rom_size);
    memcpy(rom_bak, rom, rom_size);

    emu->opts.mode = new_mode;

    size_t   save_len;
    uint8_t *save_data = emu->get_save(emu->impl, &save_len);

    emu->quit(emu->impl);

    if (!set_funcs(emu, emu->opts.mode)) {
        free(emu);
        return false;
    }

    emu->opts.rom      = rom_bak;
    emu->opts.rom_size = rom_size;
    emu->impl          = emu->init(emu);

    if (!emu->impl) {
        free(emu);
        return false;
    }

    if (save_data) {
        emu->load_save(emu->impl, save_data, save_len);
        free(save_data);
    }

    free(rom_bak);

    return true;
}

static void rewind_push(gbmulator_t *emu) {
    if (!emu)
        return;

    emu->rewind_stack.head = (emu->rewind_stack.head + 1) % N_REWIND_STATES;
    if (emu->rewind_stack.len < N_REWIND_STATES)
        emu->rewind_stack.len++;

    // TODO
    // gbmulator_savestate_t *savestate = emu->get_savestate(emu->impl, &emu->rewind_stack.state_size, false); // TODO specify the dest buffer to avoid redundant memcpy
    // memcpy(&emu->rewind_stack.states[emu->rewind_stack.head * emu->rewind_stack.state_size], savestate, emu->rewind_stack.state_size);
    // free(savestate);
}

static void rewind_pop(gbmulator_t *emu) {
    if (!emu)
        return;

    eprintf("rewind pop");
    if (emu->rewind_stack.len == 0)
        return;

    eprintf("gb_load_savestate pop %ld", emu->rewind_stack.head);
    emu->load_savestate(emu->impl, &emu->rewind_stack.states[emu->rewind_stack.head * emu->rewind_stack.state_size], emu->rewind_stack.state_size);

    emu->rewind_stack.head = emu->rewind_stack.head == 0 ? (N_REWIND_STATES - 1) : emu->rewind_stack.head - 1;
    emu->rewind_stack.len--;
    // if len == 0, set head back to -1??
}

void gbmulator_rewind(gbmulator_t *emu, uint64_t frame) {
    // TODO while rewind button is pressed, pause the emulation (do not step)
    if (emu)
        rewind_pop(emu);
}

static inline void gbmulator_step_linked(gbmulator_t *emu) {
    if (!emu)
        return;

    if (emu->rewind_stack.states) {
        static size_t step_counter = 0;
        step_counter++;
        if (step_counter == GB_CPU_STEPS_PER_FRAME) {
            step_counter = 0;
            rewind_push(emu);
        }
    }

    emu->step(emu->impl);

    if (emu->cable.other_device)
        emu->cable.other_device->step(emu->cable.other_device);
}

void gbmulator_step(gbmulator_t *emu) {
    gbmulator_step_linked(emu);
}

void gbmulator_run_steps(gbmulator_t *emu, uint64_t steps_limit) {
    if (!emu)
        return;

    for (uint64_t steps_count = 0; steps_count < steps_limit; steps_count++)
        gbmulator_step_linked(emu);
}

void gbmulator_run_frames(gbmulator_t *emu, uint64_t frames_limit) {
    gbmulator_run_steps(emu, frames_limit * GB_CPU_STEPS_PER_FRAME);
}

uint8_t *gbmulator_get_save(gbmulator_t *emu, size_t *save_length) {
    if (!emu)
        return NULL;

    return emu->get_save(emu->impl, save_length);
}

bool gbmulator_load_save(gbmulator_t *emu, uint8_t *save_data, size_t save_length) {
    if (!emu)
        return false;

    return emu->load_save(emu->impl, save_data, save_length);
}

uint8_t *gbmulator_get_savestate(gbmulator_t *emu, size_t *length, bool is_compressed) {
    if (!emu)
        return NULL;

    gbmulator_savestate_t *savestate = emu->get_savestate(emu->impl, length, is_compressed);
    if (!savestate)
        return NULL;

    return (uint8_t *) savestate;
}

bool gbmulator_load_savestate(gbmulator_t *emu, uint8_t *data, size_t length) {
    if (!emu)
        return false;

    gbmulator_savestate_t *savestate = (gbmulator_savestate_t *) data;

    if (length <= sizeof(gbmulator_savestate_t)) {
        eprintf("invalid savestate length (%zu)\n", length);
        return false;
    }

    if (strncmp(savestate->identifier, SAVESTATE_STRING, sizeof(SAVESTATE_STRING))) {
        eprintf("invalid format %s\n", savestate->identifier);
        free(savestate);
        return false;
    }
    const char *rom_title = gbmulator_get_rom_title(emu);
    if (strncmp(savestate->rom_title, rom_title, sizeof(savestate->rom_title))) {
        eprintf("rom title mismatch (expected: '%.16s'; got: '%.16s')\n", rom_title, savestate->rom_title);
        free(savestate);
        return false;
    }

    switch (savestate->mode) {
    case GBMULATOR_MODE_GB:
    case GBMULATOR_MODE_GBC:
        if (emu->opts.mode != savestate->mode) {
            if (!gbmulator_reset(emu, savestate->mode))
                return false;
        }
        break;
    case GBMULATOR_MODE_GBA:
        break;
    case GBMULATOR_MODE_GBPRINTER:
    default:
        return false;
    }

    return emu->load_savestate(emu->impl, savestate, length);
}

void gbmulator_get_options(gbmulator_t *emu, gbmulator_options_t *opts) {
    if (emu)
        *opts = emu->opts;
}

void gbmulator_set_options(gbmulator_t *emu, const gbmulator_options_t *opts) {
    if (!emu)
        return;

    // allow changes of mode, rom, rom_size and apu_sampling_rate only once (inside gbmulator_init())
    if (!emu->impl) {
        emu->opts.mode              = opts->mode;
        emu->opts.rom               = opts->rom;
        emu->opts.rom_size          = opts->rom_size;
        emu->opts.apu_sampling_rate = opts->apu_sampling_rate == 0 ? DEFAULT_APU_SAMPLING_RATE : opts->apu_sampling_rate;
    }

    emu->opts.palette                  = opts->palette;
    emu->opts.apu_speed                = MAX(opts->apu_speed, 1.0f);
    emu->opts.on_new_line              = opts->on_new_line;
    emu->opts.on_new_frame             = opts->on_new_frame;
    emu->opts.on_new_sample            = opts->on_new_sample;
    emu->opts.on_accelerometer_request = opts->on_accelerometer_request;
    emu->opts.on_camera_capture_image  = opts->on_camera_capture_image;
}

char *gbmulator_get_rom_title(gbmulator_t *emu) {
    if (!emu)
        return NULL;

    return emu->get_rom_title(emu->impl);
}

void gbmulator_print_status(gbmulator_t *emu) {
    if (emu)
        emu->print_status(emu->impl);
}

uint16_t gbmulator_get_joypad_state(gbmulator_t *emu) {
    if (!emu)
        return 0;

    return emu->get_joypad_state(emu->impl);
}

void gbmulator_set_joypad_state(gbmulator_t *emu, uint16_t state) {
    if (emu)
        emu->set_joypad_state(emu->impl, state);
}

uint8_t *gbmulator_get_rom(gbmulator_t *emu, size_t *rom_size) {
    if (!emu)
        return NULL;

    return emu->get_rom(emu->impl, rom_size);
}

void gbmulator_link_connect(gbmulator_t *emu, gbmulator_t *other, gbmulator_link_t type) {
    if (!emu)
        return;

    gbmulator_link_disconnect(emu, type);

    switch (type) {
    case GBMULATOR_LINK_CABLE:
        emu->cable.other_device                     = other;
        emu->cable.other_device->cable.other_device = emu;
        break;
    case GBMULATOR_LINK_IR:
        emu->ir.other_device                  = other;
        emu->ir.other_device->ir.other_device = emu;
        break;
    default:
        break;
    }
}

void gbmulator_link_disconnect(gbmulator_t *emu, gbmulator_link_t type) {
    if (!emu || !emu->cable.other_device)
        return;

    switch (type) {
    case GBMULATOR_LINK_CABLE:
        emu->cable.other_device->cable.other_device = NULL;
        emu->cable.other_device                     = NULL;
        break;
    case GBMULATOR_LINK_IR:
        emu->ir.other_device->ir.other_device = NULL;
        emu->ir.other_device                  = NULL;
        break;
    default:
        break;
    }
}

uint16_t gbmulator_get_rom_checksum(gbmulator_t *emu) {
    if (!emu)
        return 0;

    uint16_t checksum = 0;
    size_t   rom_size;
    uint8_t *rom = gbmulator_get_rom(emu, &rom_size);
    for (unsigned int i = 0; i < rom_size; i += 2)
        checksum = checksum - (rom[i] + rom[i + 1]) - 1;
    return checksum;
}

bool gbmulator_has_peripheral(gbmulator_t *emu, gbmulator_peripheral_t peripheral) {
    if (!emu)
        return false;

    if (emu->opts.mode != GBMULATOR_MODE_GB && emu->opts.mode != GBMULATOR_MODE_GBC)
        return false;

    switch (peripheral) {
    case GBMULATOR_PERIPHERAL_CAMERA:
        return gb_has_camera(emu->impl);
    case GBMULATOR_PERIPHERAL_ACCELEROMETER:
        return gb_has_accelerometer(emu->impl);
    default:
        return false;
    }
}

void gbmulator_set_apu_speed(gbmulator_t *emu, float speed) {
    if (emu)
        emu->opts.apu_speed = speed;
}

// TODO maybe change this
void gbmulator_set_palette(gbmulator_t *emu, gb_color_palette_t palette) {
    if (!emu || !emu->impl || (emu->opts.mode != GBMULATOR_MODE_GB && emu->opts.mode != GBMULATOR_MODE_GBC))
        return;

    emu->opts.palette = palette;
    gb_set_palette(emu->impl, palette);
}
