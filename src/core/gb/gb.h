#pragma once

#include "../utils.h"
#include "../core.h"

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
uint64_t gb_step(gb_t *gb);

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

void gb_joypad_press(gb_t *gb, gbmulator_joypad_t key);

void gb_joypad_release(gb_t *gb, gbmulator_joypad_t key);

uint16_t gb_get_joypad_state(gb_t *gb);

void gb_set_joypad_state(gb_t *gb, uint16_t state);

void gb_get_save(gb_t *gb, uint8_t *data, size_t *length);

bool gb_load_save(gb_t *gb, uint8_t *data, size_t length);

void gb_get_savestate(gb_t *gb, uint8_t *data, size_t *length);

bool gb_load_savestate(gb_t *gb, uint8_t *data, size_t length);

/**
 * @returns the ROM title (you must not free the returned pointer).
 */
char *gb_get_rom_title(gb_t *gb);

uint8_t gb_has_accelerometer(gb_t *gb);

uint8_t gb_has_camera(gb_t *gb);

void gb_set_palette(gb_t *gb, gb_color_palette_t palette);
