#pragma once

#include "../utils.h"
#include "../core.h"

#define EMULATOR_NAME "GBmulator"

#define GB_SCREEN_WIDTH 160
#define GB_SCREEN_HEIGHT 144

#define GB_CPU_FREQ 4194304
#define GB_FRAMES_PER_SECOND 60
// 4194304 cycles executed per second --> 4194304 / fps --> 4194304 / 60 == 69905 cycles per frame (the Game Boy runs at approximatively 60 fps)
#define GB_CPU_CYCLES_PER_FRAME (GB_CPU_FREQ / GB_FRAMES_PER_SECOND)
#define GB_CPU_STEPS_PER_FRAME (GB_CPU_CYCLES_PER_FRAME / 4)

#define GB_CAMERA_SENSOR_WIDTH 128
#define GB_CAMERA_SENSOR_HEIGHT 128

typedef struct gb_t gb_t;

typedef enum {
    DMG_WHITE,
    DMG_LIGHT_GRAY,
    DMG_DARK_GRAY,
    DMG_BLACK
} gb_dmg_color_t;

/**
 * Runs the emulator for one cpu step. If `gb` is linked to another device, it is also run for one step.
 * @returns the amount of cycles the emulator has run for
 */
void gb_step(gb_t *gb);

int gb_is_rom_valid(const uint8_t *rom);

/**
 * Inits the emulator.
 * @param base pointer to a base gbmulator instance.
 */
gb_t *gb_init(gbmulator_t *base);

/**
 * Quits the emulator gracefully (save eram into a '.sav' file, ...).
 */
void gb_quit(gb_t *gb);

void gb_print_status(gb_t *gb);

uint8_t gb_link_shift_bit(gb_t *gb, uint8_t in_bit);

void gb_link_data_received(gb_t *gb);

void gb_joypad_press(gb_t *gb, gbmulator_joypad_button_t key);

void gb_joypad_release(gb_t *gb, gbmulator_joypad_button_t key);

uint16_t gb_get_joypad_state(gb_t *gb);

void gb_set_joypad_state(gb_t *gb, uint16_t state);

uint8_t *gb_get_save(gb_t *gb, size_t *save_length);

bool gb_load_save(gb_t *gb, uint8_t *save_data, size_t save_length);

gbmulator_savestate_t *gb_get_savestate(gb_t *gb, size_t *savestate_data_length, bool is_compressed);

bool gb_load_savestate(gb_t *gb, gbmulator_savestate_t *savestate, size_t savestate_data_length);

/**
 * @returns the ROM title (you must not free the returned pointer).
 */
char *gb_get_rom_title(gb_t *gb);

/**
 * @returns a pointer to the ROM (you must not free the returned pointer).
 */
uint8_t *gb_get_rom(gb_t *gb, size_t *rom_size);

uint8_t gb_has_accelerometer(gb_t *gb);

uint8_t gb_has_camera(gb_t *gb);

void gb_set_palette(gb_t *gb, gb_color_palette_t palette);
