#pragma once

#include "core.h"

typedef void *(*init_func_t)(gbmulator_t *base);
typedef void (*quit_func_t)(void *impl);
typedef void (*step_func_t)(void *impl);
typedef uint8_t *(*get_save_func_t)(void *impl, size_t *save_length);
typedef bool (*load_save_func_t)(void *impl, uint8_t *save_data, size_t save_length);
typedef gbmulator_savestate_t *(*get_savestate_func_t)(void *impl, size_t *savestate_length, bool is_compressed);
typedef bool (*load_savestate_func_t)(void *impl, gbmulator_savestate_t *data, size_t savestate_length);
typedef char *(*get_rom_title_func_t)(void *impl);
typedef void (*print_status_func_t)(void *impl);
typedef uint16_t (*get_joypad_state_func_t)(void *impl);
typedef void (*set_joypad_state_func_t)(void *impl, uint16_t state);
typedef uint8_t *(*get_rom_func_t)(void *impl, size_t *rom_size);

typedef uint8_t (*cable_shift_bit_cb_t)(void *impl, uint8_t in_bit);
typedef void (*cable_data_received_cb_t)(void *impl);

struct gbmulator_t {
    gbmulator_options_t opts;
    void               *impl;

    init_func_t             init;
    quit_func_t             quit;
    step_func_t             step;
    get_save_func_t         get_save;
    load_save_func_t        load_save;
    get_savestate_func_t    get_savestate;
    load_savestate_func_t   load_savestate;
    get_rom_title_func_t    get_rom_title;
    print_status_func_t     print_status;
    get_joypad_state_func_t get_joypad_state;
    set_joypad_state_func_t set_joypad_state;
    get_rom_func_t          get_rom;

    struct {
        gbmulator_t             *other_device;
        cable_shift_bit_cb_t     shift_bit;
        cable_data_received_cb_t data_received;
    } cable;

    struct {
        gbmulator_t *other_device;
    } ir;

    // TODO
    // - rollback netplay: don't wait other emu inputs, predict them (keep the previously received input), then when we received the inputs, rollback to the last known synchronized state for both the local emu and the local copy of the remote emu --> last known sync state is the state just BEFORE we started predicting, THEN we apply the input we just received by the remote
    //   WHAT happens if then, the remote doesn't reveive the inputs of the emulator that is ahead? it has to wait? or it just does the same: predict input + rollback
    //     won't this cause deadlocks/loops of rollbacks without progressing/high cpu usage?

    // - TO do this, add rewind gameplay feature when R key held, rewind until 16 seconds ??? (fine tune this value: maybe less is preferable)
    //     for rollback netplay, disallow rewind gameplay BUT use the same code / mechanism to rollback (once we have more than max buffer of previous states, sleep the emulation to wait for the other one to finally respond)
    struct {
        gbmulator_savestate_t *states;
        ssize_t                head;
        size_t                 len;
        size_t                 state_size; // all states for a same ROM are the same size
    } rewind_stack;
};
