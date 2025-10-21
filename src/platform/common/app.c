#include <stdio.h>

#include "app.h"
#include "utils.h"
#include "glrenderer.h"
#include "alrenderer.h"

#include "../../core/core.h"

#ifndef __attribute_used__
#define __attribute_used__ __attribute__((__used__))
#endif

struct {
    bool          is_paused;
    glrenderer_t *renderer;
    char          window_title[sizeof(EMULATOR_NAME) + 19];
    uint8_t       joypad_state;
    gbmulator_t  *emu;
    config_t      config;
} app;

static void load_config(void) {
    char *path       = get_config_path();
    config_load_from_file(&app.config, path);
    free(path);
}

static void save_config(void) {
    char *path       = get_config_path();
    config_save_to_file(&app.config, path);
    free(path);
}

static void load() {
    if (!app.emu)
        return;

    char *rom_title = gbmulator_get_rom_title(app.emu);
    char *path      = get_save_path(rom_title);

    load_battery_from_file(app.emu, path);

    free(path);
}

static void save(void) {
    if (!app.emu)
        return;

    char *rom_title = gbmulator_get_rom_title(app.emu);
    char *path      = get_save_path(rom_title);

    save_battery_to_file(app.emu, path);

    free(path);
}

__attribute_used__ void app_init(config_t *default_config) {
    memset(&app, 0, sizeof(app));

    app.config = *default_config;
    load_config();

    app.is_paused    = true;
    app.joypad_state = 0xFF;

    size_t screen_w;
    size_t screen_h;
    if (app.config.mode == GBMULATOR_MODE_GBA) {
        screen_w = GBA_SCREEN_WIDTH;
        screen_h = GBA_SCREEN_HEIGHT;
    } else {
        screen_w = GB_SCREEN_WIDTH;
        screen_h = GB_SCREEN_HEIGHT;
    }

    app.renderer = glrenderer_init(screen_w, screen_h, false);

    alrenderer_init(0);
    alrenderer_enable_dynamic_rate_control(app.config.sound_drc);
    app_set_sound(app.config.sound);
}

__attribute_used__ void app_quit(void) {
    if (app.emu) {
        save();
        gbmulator_quit(app.emu);
    }

    save_config();

    alrenderer_quit();
    glrenderer_quit(app.renderer);
}

__attribute_used__ void app_reset(void) {
    if (!app.emu)
        return;

    save();

    gbmulator_reset(app.emu, app.config.mode);
    alrenderer_clear_queue();
}

__attribute_used__ void app_loop(void) {
    gbmulator_set_joypad_state(app.emu, app.joypad_state);

    // run the emulator for the approximate number of steps it takes for the ppu to render a frame
    gbmulator_run_steps(app.emu, GB_CPU_STEPS_PER_FRAME * app.config.speed); // TODO if gba steps per frame

    // TODO should not be in this loop because desktop platform can only do this during a gl area render
    glrenderer_render(app.renderer);
}

__attribute_used__ void app_joypad_press(gbmulator_joypad_button_t button) {
    if (!app.is_paused && app.emu)
        RESET_BIT(app.joypad_state, button);
}

__attribute_used__ void app_joypad_release(gbmulator_joypad_button_t button) {
    if (!app.is_paused && app.emu)
        SET_BIT(app.joypad_state, button);
}

static void on_new_frame_cb(const uint8_t *pixels) {
    glrenderer_update_screen(app.renderer, pixels);
}

__attribute_used__ void app_load_cartridge(uint8_t *rom, size_t rom_size) {
    gbmulator_options_t opts = {
        .rom               = rom,
        .rom_size          = rom_size,
        .mode              = app.config.mode,
        .on_new_sample     = alrenderer_queue_sample,
        .on_new_frame      = on_new_frame_cb,
        .apu_speed         = app.config.speed,
        .apu_sampling_rate = alrenderer_get_sampling_rate(),
        .palette           = app.config.color_palette
    };
    gbmulator_t *new_emu = gbmulator_init(&opts);
    if (!new_emu)
        return;

    if (app.emu) {
        save();
        gbmulator_quit(app.emu);
    }
    app.emu         = new_emu;
    char *rom_title = gbmulator_get_rom_title(app.emu);
    alrenderer_clear_queue();

    load();

    gbmulator_print_status(app.emu);
}

__attribute_used__ void app_set_pause(bool is_paused) {
    app.is_paused = is_paused;
}

__attribute_used__ void app_set_sound(float value) {
    app.config.sound = value;
    alrenderer_set_level(app.config.sound);
}

__attribute_used__ void app_set_mode(gbmulator_mode_t mode) {
    app.config.mode = mode;
}

__attribute_used__ void app_set_speed(float value) {
    app.config.speed = value;
    // TODO
    // gbmulator_set_apu_speed(emu, config.speed);
}

__attribute_used__ void app_set_palette(gb_color_palette_t value) {
    // TODO
    app.config.color_palette = value;
    // if (emu)
    //     gb_set_palette(emu, config.color_palette);
}

__attribute_used__ void app_set_size(uint32_t screen_w, uint32_t screen_h, uint32_t viewport_w, uint32_t viewport_h) {
    glrenderer_resize_screen(app.renderer, screen_w, screen_h);
    glrenderer_resize_viewport(app.renderer, viewport_w, viewport_h);
}

__attribute_used__ void app_get_config(config_t *config) {
    if (config)
        *config = app.config;
}

gbmulator_joypad_button_t app_keycode_to_joypad(unsigned int keycode) {
    if (keycode == app.config.keybindings[JOYPAD_A])
        return JOYPAD_A;
    if (keycode == app.config.keybindings[JOYPAD_B])
        return JOYPAD_B;
    if (keycode == app.config.keybindings[JOYPAD_SELECT])
        return JOYPAD_SELECT;
    if (keycode == app.config.keybindings[JOYPAD_START])
        return JOYPAD_START;
    if (keycode == app.config.keybindings[JOYPAD_RIGHT])
        return JOYPAD_RIGHT;
    if (keycode == app.config.keybindings[JOYPAD_LEFT])
        return JOYPAD_LEFT;
    if (keycode == app.config.keybindings[JOYPAD_UP])
        return JOYPAD_UP;
    if (keycode == app.config.keybindings[JOYPAD_DOWN])
        return JOYPAD_DOWN;
    if (keycode == app.config.keybindings[JOYPAD_R])
        return JOYPAD_R;
    if (keycode == app.config.keybindings[JOYPAD_L])
        return JOYPAD_L;
    return -1;
}

gbmulator_joypad_button_t app_button_to_joypad(unsigned int button) {
    if (button == app.config.gamepad_bindings[JOYPAD_A])
        return JOYPAD_A;
    if (button == app.config.gamepad_bindings[JOYPAD_B])
        return JOYPAD_B;
    if (button == app.config.gamepad_bindings[JOYPAD_SELECT])
        return JOYPAD_SELECT;
    if (button == app.config.gamepad_bindings[JOYPAD_START])
        return JOYPAD_START;
    if (button == app.config.gamepad_bindings[JOYPAD_RIGHT])
        return JOYPAD_RIGHT;
    if (button == app.config.gamepad_bindings[JOYPAD_LEFT])
        return JOYPAD_LEFT;
    if (button == app.config.gamepad_bindings[JOYPAD_UP])
        return JOYPAD_UP;
    if (button == app.config.gamepad_bindings[JOYPAD_DOWN])
        return JOYPAD_DOWN;
    if (button == app.config.gamepad_bindings[JOYPAD_R])
        return JOYPAD_R;
    if (button == app.config.gamepad_bindings[JOYPAD_L])
        return JOYPAD_L;
    return -1;
}
