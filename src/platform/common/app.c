#include <stdio.h>

#include "app.h"
#include "utils.h"
#include "link.h"
#include "glrenderer.h"
#include "alrenderer.h"

#include "../../core/core.h"

#ifndef __attribute_used__
#define __attribute_used__ __attribute__((__used__))
#endif

#define MAX_TOUCHES 32

static struct {
    bool                is_paused;
    bool                is_rewinding;
    uint32_t            steps_per_frame;
    glrenderer_t       *renderer;
    uint16_t            joypad_state;
    gbmulator_t        *emu;
    gbmulator_t        *linked_emu;
    int                 sfd;
    config_t            config;
    uint8_t             joypad_touch_counter[GBMULATOR_JOYPAD_END];
    bool                joypad_key_press_counter[GBMULATOR_JOYPAD_END];
    glrenderer_obj_id_t touches_current_obj[32];
} app;

static void set_steps_per_frame(void) {
    switch (app.config.mode) {
    case GBMULATOR_MODE_GB:
    case GBMULATOR_MODE_GBC:
        app.steps_per_frame = GB_CPU_STEPS_PER_FRAME * app.config.speed;
        break;
    case GBMULATOR_MODE_GBA:
        app.steps_per_frame = GBA_CPU_STEPS_PER_FRAME * app.config.speed;
        break;
    case GBMULATOR_MODE_GBPRINTER:
    default:
        app.steps_per_frame = 0;
        break;
    }
}

static gbmulator_joypad_t app_keycode_to_joypad(unsigned int keycode) {
    if (keycode == app.config.keybindings[GBMULATOR_JOYPAD_A])
        return GBMULATOR_JOYPAD_A;
    if (keycode == app.config.keybindings[GBMULATOR_JOYPAD_B])
        return GBMULATOR_JOYPAD_B;
    if (keycode == app.config.keybindings[GBMULATOR_JOYPAD_SELECT])
        return GBMULATOR_JOYPAD_SELECT;
    if (keycode == app.config.keybindings[GBMULATOR_JOYPAD_START])
        return GBMULATOR_JOYPAD_START;
    if (keycode == app.config.keybindings[GBMULATOR_JOYPAD_RIGHT])
        return GBMULATOR_JOYPAD_RIGHT;
    if (keycode == app.config.keybindings[GBMULATOR_JOYPAD_LEFT])
        return GBMULATOR_JOYPAD_LEFT;
    if (keycode == app.config.keybindings[GBMULATOR_JOYPAD_UP])
        return GBMULATOR_JOYPAD_UP;
    if (keycode == app.config.keybindings[GBMULATOR_JOYPAD_DOWN])
        return GBMULATOR_JOYPAD_DOWN;
    if (keycode == app.config.keybindings[GBMULATOR_JOYPAD_R])
        return GBMULATOR_JOYPAD_R;
    if (keycode == app.config.keybindings[GBMULATOR_JOYPAD_L])
        return GBMULATOR_JOYPAD_L;
    return -1;
}

static gbmulator_joypad_t app_button_to_joypad(unsigned int button) {
    if (button == app.config.gamepad_bindings[GBMULATOR_JOYPAD_A])
        return GBMULATOR_JOYPAD_A;
    if (button == app.config.gamepad_bindings[GBMULATOR_JOYPAD_B])
        return GBMULATOR_JOYPAD_B;
    if (button == app.config.gamepad_bindings[GBMULATOR_JOYPAD_SELECT])
        return GBMULATOR_JOYPAD_SELECT;
    if (button == app.config.gamepad_bindings[GBMULATOR_JOYPAD_START])
        return GBMULATOR_JOYPAD_START;
    if (button == app.config.gamepad_bindings[GBMULATOR_JOYPAD_RIGHT])
        return GBMULATOR_JOYPAD_RIGHT;
    if (button == app.config.gamepad_bindings[GBMULATOR_JOYPAD_LEFT])
        return GBMULATOR_JOYPAD_LEFT;
    if (button == app.config.gamepad_bindings[GBMULATOR_JOYPAD_UP])
        return GBMULATOR_JOYPAD_UP;
    if (button == app.config.gamepad_bindings[GBMULATOR_JOYPAD_DOWN])
        return GBMULATOR_JOYPAD_DOWN;
    if (button == app.config.gamepad_bindings[GBMULATOR_JOYPAD_R])
        return GBMULATOR_JOYPAD_R;
    if (button == app.config.gamepad_bindings[GBMULATOR_JOYPAD_L])
        return GBMULATOR_JOYPAD_L;
    return -1;
}

static inline void btn_press(gbmulator_joypad_t button, bool is_touch) {
    if (button < 0 || button >= GBMULATOR_JOYPAD_END)
        return;

    if (is_touch)
        app.joypad_touch_counter[button]++;
    else
        app.joypad_key_press_counter[button] = true;

    if (app.joypad_touch_counter[button] + app.joypad_key_press_counter[button] == 1) {
        RESET_BIT(app.joypad_state, button);
        glrenderer_set_obj_tint(app.renderer, (glrenderer_obj_id_t) button, 0.5f);
    }
}

static inline void btn_release(gbmulator_joypad_t button, bool is_touch) {
    if (button < 0 || button >= GBMULATOR_JOYPAD_END)
        return;

    if (is_touch)
        app.joypad_touch_counter[button]--;
    else
        app.joypad_key_press_counter[button] = false;

    if (app.joypad_touch_counter[button] > 0)
        app.joypad_touch_counter[button]--;

    if (app.joypad_touch_counter[button] + app.joypad_key_press_counter[button] == 0) {
        SET_BIT(app.joypad_state, button);
        glrenderer_set_obj_tint(app.renderer, (glrenderer_obj_id_t) button, 1.0f);
    }
}

__attribute_used__ void app_init(config_t *default_config) {
    memset(&app, 0, sizeof(app));

    app.config = *default_config;
    config_load_from_file(&app.config, get_config_path());

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

    for (int i = 0; i < MAX_TOUCHES; i++)
        app.touches_current_obj[i] = GLRENDERER_OBJ_ID_SCREEN;

    set_steps_per_frame();

    app.renderer = glrenderer_init(screen_w, screen_h, false);

    alrenderer_init(0);
    alrenderer_enable_dynamic_rate_control(app.config.sound_drc);
    app_set_sound(app.config.sound);
}

__attribute_used__ void app_quit(void) {
    if (app.emu) {
        save_battery_to_file(app.emu, get_save_path(gbmulator_get_rom_title(app.emu)));

        gbmulator_quit(app.emu);
    }

    config_save_to_file(&app.config, get_config_path());

    alrenderer_quit();
    glrenderer_quit(app.renderer);
}

__attribute_used__ void app_reset(void) {
    if (!app.emu)
        return;

    save_battery_to_file(app.emu, get_save_path(gbmulator_get_rom_title(app.emu)));

    gbmulator_reset(app.emu, app.config.mode);
    gbmulator_print_status(app.emu);
    alrenderer_clear_queue();
}

__attribute_used__ void app_run_frame(void) {
    if (app.is_paused)
        return;

    if (app.is_rewinding) {
        gbmulator_rewind(app.emu, -1);
    } else {
        gbmulator_set_joypad_state(app.emu, app.joypad_state);

        // TODO async or timeout link_exchange_joypad to avoid blocking the gui
        if (app.linked_emu) {
            if (!link_exchange_joypad(app.sfd, app.emu, app.linked_emu)) {
                app.linked_emu = NULL;
                app.sfd        = -1;
                // set_link_gui_actions(TRUE, TRUE);
                set_steps_per_frame();
                // gbmulator_options_t opts;
                // gbmulator_get_options(app.emu, &opts);
                // opts.apu_speed = app.config.speed;
                // gbmulator_set_options(app.emu, &opts);
                // show_toast("Link Cable disconnected");
            }
        }
        gbmulator_run_steps(app.emu, app.steps_per_frame);
    }
}

__attribute_used__ void app_render(void) {
    glrenderer_render(app.renderer);
}

__attribute_used__ void app_keyboard_press(unsigned int key) {
    if (app.is_paused || !app.emu)
        return;

    btn_press(app_keycode_to_joypad(key), false);
}

__attribute_used__ void app_keyboard_release(unsigned int key) {
    if (app.is_paused || !app.emu)
        return;

    btn_release(app_keycode_to_joypad(key), false);
}

__attribute_used__ void app_gamepad_press(unsigned int button) {
    if (app.is_paused || !app.emu)
        return;

    btn_press(app_button_to_joypad(button), false);
}

__attribute_used__ void app_gamepad_release(unsigned int button) {
    if (app.is_paused || !app.emu)
        return;

    btn_release(app_button_to_joypad(button), false);
}

static inline void btn_touch_press(glrenderer_obj_id_t obj_id) {
    switch (obj_id) {
    case GLRENDERER_OBJ_ID_DPAD_UP_RIGHT:
        btn_press(GBMULATOR_JOYPAD_UP, true);
        btn_press(GBMULATOR_JOYPAD_RIGHT, true);
        break;
    case GLRENDERER_OBJ_ID_DPAD_UP_LEFT:
        btn_press(GBMULATOR_JOYPAD_UP, true);
        btn_press(GBMULATOR_JOYPAD_LEFT, true);
        break;
    case GLRENDERER_OBJ_ID_DPAD_DOWN_RIGHT:
        btn_press(GBMULATOR_JOYPAD_DOWN, true);
        btn_press(GBMULATOR_JOYPAD_RIGHT, true);
        break;
    case GLRENDERER_OBJ_ID_DPAD_DOWN_LEFT:
        btn_press(GBMULATOR_JOYPAD_DOWN, true);
        btn_press(GBMULATOR_JOYPAD_LEFT, true);
        break;
    case GLRENDERER_OBJ_ID_LINK:
    case GLRENDERER_OBJ_ID_DPAD_CENTER:
    case GLRENDERER_OBJ_ID_SCREEN:
        break;
    default:
        btn_press((gbmulator_joypad_t) obj_id, true);
        break;
    }
}

static inline void btn_touch_release(glrenderer_obj_id_t obj_id) {
    switch (obj_id) {
    case GLRENDERER_OBJ_ID_DPAD_UP_RIGHT:
        btn_release(GBMULATOR_JOYPAD_UP, true);
        btn_release(GBMULATOR_JOYPAD_RIGHT, true);
        break;
    case GLRENDERER_OBJ_ID_DPAD_UP_LEFT:
        btn_release(GBMULATOR_JOYPAD_UP, true);
        btn_release(GBMULATOR_JOYPAD_LEFT, true);
        break;
    case GLRENDERER_OBJ_ID_DPAD_DOWN_RIGHT:
        btn_release(GBMULATOR_JOYPAD_DOWN, true);
        btn_release(GBMULATOR_JOYPAD_RIGHT, true);
        break;
    case GLRENDERER_OBJ_ID_DPAD_DOWN_LEFT:
        btn_release(GBMULATOR_JOYPAD_DOWN, true);
        btn_release(GBMULATOR_JOYPAD_LEFT, true);
        break;
    case GLRENDERER_OBJ_ID_LINK:
    case GLRENDERER_OBJ_ID_DPAD_CENTER:
    case GLRENDERER_OBJ_ID_SCREEN:
        break;
    default:
        btn_release((gbmulator_joypad_t) obj_id, true);
        break;
    }
}

__attribute_used__ void app_touch_press(uint8_t touch_id, uint32_t x, uint32_t y) {
    if (app.is_paused || !app.emu || touch_id >= MAX_TOUCHES)
        return;

    glrenderer_obj_id_t obj_id = glrenderer_get_obj_at_coord(app.renderer, x, y);

    btn_touch_press(obj_id);

    app.touches_current_obj[touch_id] = obj_id;
}

__attribute_used__ void app_touch_release(uint8_t touch_id, uint32_t x, uint32_t y) {
    if (app.is_paused || !app.emu || touch_id >= MAX_TOUCHES)
        return;

    btn_touch_release(app.touches_current_obj[touch_id]);

    app.touches_current_obj[touch_id] = GLRENDERER_OBJ_ID_SCREEN;
}

__attribute_used__ void app_touch_move(uint8_t touch_id, uint32_t x, uint32_t y) {
    if (app.is_paused || !app.emu || touch_id >= MAX_TOUCHES)
        return;

    glrenderer_obj_id_t obj_id = glrenderer_get_obj_at_coord(app.renderer, x, y);

    if (obj_id != app.touches_current_obj[touch_id]) {
        btn_touch_release(app.touches_current_obj[touch_id]);
        btn_touch_press(obj_id);
    }

    app.touches_current_obj[touch_id] = obj_id;
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
        save_battery_to_file(app.emu, get_save_path(gbmulator_get_rom_title(app.emu)));
        gbmulator_quit(app.emu);
    }
    app.emu = new_emu;
    alrenderer_clear_queue();

    load_battery_from_file(app.emu, get_save_path(gbmulator_get_rom_title(app.emu)));

    set_steps_per_frame();
    gbmulator_print_status(app.emu);
}

__attribute_used__ void app_set_pause(bool is_paused) {
    app.is_paused = is_paused;
}

__attribute_used__ void app_set_sound(float value) {
    app.config.sound = value;
    alrenderer_set_level(app.config.sound);
}

__attribute_used__ gbmulator_mode_t app_get_mode(void) {
    if (!app.emu)
        return app.config.mode;

    gbmulator_options_t opts;
    gbmulator_get_options(app.emu, &opts);

    return opts.mode;
}

__attribute_used__ bool app_set_mode(gbmulator_mode_t mode) {
    app.config.mode = mode;
    return app.emu == NULL;
}

__attribute_used__ void app_set_speed(float value) {
    app.config.speed = value;
    set_steps_per_frame();
    gbmulator_set_apu_speed(app.emu, app.config.speed);
}

__attribute_used__ void app_set_palette(gb_color_palette_t value) {
    app.config.color_palette = value;
    gbmulator_set_palette(app.emu, value);
}

__attribute_used__ void app_get_screen_size(uint32_t *screen_w, uint32_t *screen_h) {
    if (!screen_w || !screen_h)
        return;

    switch (app.config.mode) {
    case GBMULATOR_MODE_GB:
    case GBMULATOR_MODE_GBC:
        *screen_w = GB_SCREEN_WIDTH;
        *screen_h = GB_SCREEN_HEIGHT;
        break;
    case GBMULATOR_MODE_GBA:
        *screen_w = GBA_SCREEN_WIDTH;
        *screen_h = GBA_SCREEN_HEIGHT;
        break;
    default:
        eprintf("%d is not a valid mode\n", app.config.mode);
        return;
    }
}

__attribute_used__ void app_set_size(uint32_t viewport_w, uint32_t viewport_h) {
    uint32_t screen_w;
    uint32_t screen_h;
    app_get_screen_size(&screen_w, &screen_h);

    glrenderer_resize_screen(app.renderer, screen_w, screen_h);
    glrenderer_resize_viewport(app.renderer, viewport_w, viewport_h);
}

__attribute_used__ void app_get_config(config_t *config) {
    if (config)
        *config = app.config;
}

__attribute_used__ bool app_is_paused(void) {
    return app.is_paused;
}

__attribute_used__ void app_save_state(int slot) {
    if (app.emu)
        save_state_to_file(app.emu, get_savestate_path(gbmulator_get_rom_title(app.emu), slot), true);
}

__attribute_used__ void app_load_state(int slot) {
    if (app.emu)
        load_state_from_file(app.emu, get_savestate_path(gbmulator_get_rom_title(app.emu), slot));
}

__attribute_used__ uint32_t app_get_fps(void) {
    switch (app.config.mode) {
    case GBMULATOR_MODE_GB:
    case GBMULATOR_MODE_GBC:
        return GB_FRAMES_PER_SECOND;
    case GBMULATOR_MODE_GBA:
        return GBA_FRAMES_PER_SECOND;
    case GBMULATOR_MODE_GBPRINTER:
    default:
        return 0;
    }
}

__attribute_used__ const char *app_get_rom_title(void) {
    return gbmulator_get_rom_title(app.emu);
}

__attribute_used__ void app_set_touchscreen_mode(bool enable) {
    uint32_t visible_btns = 0;

    if (enable) {
        visible_btns |= (1 << GLRENDERER_OBJ_ID_A) | (1 << GLRENDERER_OBJ_ID_B) | (1 << GLRENDERER_OBJ_ID_SELECT) |
                        (1 << GLRENDERER_OBJ_ID_START) | (1 << GLRENDERER_OBJ_ID_DPAD_RIGHT) |
                        (1 << GLRENDERER_OBJ_ID_DPAD_LEFT) | (1 << GLRENDERER_OBJ_ID_DPAD_UP) |
                        (1 << GLRENDERER_OBJ_ID_DPAD_DOWN) | (1 << GLRENDERER_OBJ_ID_DPAD_CENTER) |
                        (1 << GLRENDERER_OBJ_ID_DPAD_UP_RIGHT) | (1 << GLRENDERER_OBJ_ID_DPAD_UP_LEFT) |
                        (1 << GLRENDERER_OBJ_ID_DPAD_DOWN_RIGHT) | (1 << GLRENDERER_OBJ_ID_DPAD_DOWN_LEFT);

        // TODO (1 << GLRENDERER_OBJ_ID_LINK)

        if (app_get_mode() == GBMULATOR_MODE_GBA)
            visible_btns |= (1 << GLRENDERER_OBJ_ID_R) | (1 << GLRENDERER_OBJ_ID_L);
    }

    glrenderer_set_show_buttons(app.renderer, visible_btns);
}
