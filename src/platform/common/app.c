#include <stdio.h>
#include <unistd.h>

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
    bool                  is_paused;
    bool                  is_rewinding;
    uint32_t              steps_per_frame;
    glrenderer_t         *renderer;
    glrenderer_t         *printer_renderer;
    uint16_t              joypad_state;
    gbmulator_t          *emu;
    gbmulator_t          *linked_emu;
    gbmulator_t          *printer;
    int                   sfd;
    uint32_t              printer_height;
    printer_new_line_cb_t on_printer_new_line;
    config_t              config;
    uint8_t               joypad_touch_counter[GBMULATOR_JOYPAD_END];
    bool                  joypad_key_press_counter[GBMULATOR_JOYPAD_END];
    glrenderer_obj_id_t   touches_current_obj[32];
} app;

static void set_steps_per_frame(void) {
    float speed = app.linked_emu ? 1.0f : app.config.speed;

    switch (app.config.mode) {
    case GBMULATOR_MODE_GB:
    case GBMULATOR_MODE_GBC:
        app.steps_per_frame = GB_CPU_STEPS_PER_FRAME * speed;
        break;
    case GBMULATOR_MODE_GBA:
        app.steps_per_frame = GBA_CPU_STEPS_PER_FRAME * speed;
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

// TODO a bit buggy when combining clicks (touches) and keyboard input
static inline void btn_press(gbmulator_joypad_t joypad, bool is_touch) {
    if (joypad < 0 || joypad >= GBMULATOR_JOYPAD_END)
        return;

    if (is_touch)
        app.joypad_touch_counter[joypad]++;
    else
        app.joypad_key_press_counter[joypad] = true;

    if (app.joypad_touch_counter[joypad] + app.joypad_key_press_counter[joypad] == 1) {
        RESET_BIT(app.joypad_state, joypad);
        glrenderer_set_obj_tint(app.renderer, (glrenderer_obj_id_t) joypad, 0.5f);
    }
}

static inline void btn_release(gbmulator_joypad_t joypad, bool is_touch) {
    if (joypad < 0 || joypad >= GBMULATOR_JOYPAD_END)
        return;

    if (is_touch)
        app.joypad_touch_counter[joypad]--;
    else
        app.joypad_key_press_counter[joypad] = false;

    if (app.joypad_touch_counter[joypad] > 0)
        app.joypad_touch_counter[joypad]--;

    if (app.joypad_touch_counter[joypad] + app.joypad_key_press_counter[joypad] == 0) {
        SET_BIT(app.joypad_state, joypad);
        glrenderer_set_obj_tint(app.renderer, (glrenderer_obj_id_t) joypad, 1.0f);
    }
}

static inline void apply_config(void) {
    app_set_mode(app.config.mode);

    app_set_speed(app.config.speed);

    alrenderer_enable_dynamic_rate_control(app.config.sound_drc);
    app_set_sound(app.config.sound);

    app_set_touchscreen_mode(app.config.enable_joypad);
}

__attribute_used__ void app_init(void) {
    static config_t config_bak;
    config_bak = app.config;

    memset(&app, 0, sizeof(app));

    app.config = config_bak;

    app_set_pause(true);

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

    app.renderer = glrenderer_init(screen_w, screen_h, 0);

    alrenderer_init(0);

    apply_config();

    app.sfd = -1;

    eprintf("APP_INIT DONE");
}

__attribute_used__ void app_load_config(const config_t *default_config) {
    memset(&app.config, 0, sizeof(app.config));

    if (default_config)
        app.config = *default_config;
    config_load_from_file(&app.config, get_config_path());

    // TODO only allow gb/gbc/gba modes

    apply_config();
}

__attribute_used__ void app_quit(void) {
    if (app.emu) {
        save_battery_to_file(app.emu, get_save_path(gbmulator_get_rom_title(app.emu)));

        gbmulator_quit(app.emu);
        app.emu = NULL;
    }

    if (app.linked_emu) {
        gbmulator_quit(app.linked_emu);
        app.linked_emu = NULL;
    }

    if (app.printer) {
        gbmulator_quit(app.printer);
        app.printer = NULL;
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
                app_link_disconnect();
                set_steps_per_frame();
                // TODO callback to notify gui
                // set_link_gui_actions(TRUE, TRUE);
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
    btn_press(app_keycode_to_joypad(key), false);
}

__attribute_used__ void app_keyboard_release(unsigned int key) {
    btn_release(app_keycode_to_joypad(key), false);
}

__attribute_used__ void app_gamepad_press(unsigned int button) {
    btn_press(app_button_to_joypad(button), false);
}

__attribute_used__ void app_gamepad_release(unsigned int button) {
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
    if (touch_id >= MAX_TOUCHES)
        return;

    glrenderer_obj_id_t obj_id = glrenderer_get_obj_at_coord(app.renderer, x, y);

    btn_touch_press(obj_id);

    app.touches_current_obj[touch_id] = obj_id;
}

__attribute_used__ void app_touch_release(uint8_t touch_id, uint32_t x, uint32_t y) {
    if (touch_id >= MAX_TOUCHES)
        return;

    btn_touch_release(app.touches_current_obj[touch_id]);

    app.touches_current_obj[touch_id] = GLRENDERER_OBJ_ID_SCREEN;
}

__attribute_used__ void app_touch_move(uint8_t touch_id, uint32_t x, uint32_t y) {
    if (touch_id >= MAX_TOUCHES)
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

__attribute_used__ bool app_load_cartridge(uint8_t *rom, size_t rom_size) {
    if (!app.renderer)
        return false;

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
    if (!new_emu) {
        if (opts.mode == GBMULATOR_MODE_GBA)
            opts.mode = GBMULATOR_MODE_GBC;
        else
            opts.mode = GBMULATOR_MODE_GBA;

        new_emu = gbmulator_init(&opts);
        if (!new_emu)
            return false;
    }

    app.config.mode = opts.mode;
    apply_config();

    if (app.emu) {
        save_battery_to_file(app.emu, get_save_path(gbmulator_get_rom_title(app.emu)));
        gbmulator_quit(app.emu);
    }
    app.emu = new_emu;
    alrenderer_clear_queue();

    load_battery_from_file(app.emu, get_save_path(gbmulator_get_rom_title(app.emu)));

    gbmulator_print_status(app.emu);

    return true;
}

__attribute_used__ void app_set_pause(bool is_paused) {
    app.is_paused = is_paused;

    if (app.is_paused)
        alrenderer_pause();
    else
        alrenderer_play();
}

__attribute_used__ void app_set_sound(float value) {
    app.config.sound = CLAMP(value, 0.0f, 1.0f);
    alrenderer_set_level(app.config.sound);
}

__attribute_used__ void app_set_drc(bool is_enabled) {
    app.config.sound_drc = is_enabled;
    alrenderer_enable_dynamic_rate_control(app.config.sound_drc);
}

__attribute_used__ gbmulator_mode_t app_get_mode(void) {
    if (!app.emu)
        return app.config.mode;

    gbmulator_options_t opts;
    gbmulator_get_options(app.emu, &opts);

    return opts.mode;
}

// TODO every config setter should do value bounds check
__attribute_used__ bool app_set_mode(gbmulator_mode_t mode) {
    if (mode < 0 || mode >= GBMULATOR_MODE_GBPRINTER)
        return false;

    app.config.mode = mode;

    if (!app.emu) {
        uint32_t screen_w;
        uint32_t screen_h;
        app_get_screen_size(&screen_w, &screen_h);
        glrenderer_resize_screen(app.renderer, screen_w, screen_h);

        return true;
    }

    return false;
}

__attribute_used__ void app_set_speed(float value) {
    app.config.speed = CLAMP(value, 1.0f, APP_MAX_SPEED);
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
        exit(EXIT_FAILURE);
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
    if (app.emu && !app.linked_emu)
        save_state_to_file(app.emu, get_savestate_path(gbmulator_get_rom_title(app.emu), slot), true);
}

__attribute_used__ void app_load_state(int slot) {
    if (app.emu && !app.linked_emu)
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
    app.config.enable_joypad = enable;

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
    app_set_joypad_opacity(app.config.joypad_opacity);
}

__attribute_used__ void app_set_joypad_opacity(float value) {
    app.config.joypad_opacity = CLAMP(value, 0.0f, 1.0f);
    for (glrenderer_obj_id_t obj_id = 0; obj_id < GLRENDERER_OBJ_ID_SCREEN; obj_id++)
        glrenderer_set_obj_alpha(app.renderer, obj_id, app.config.joypad_opacity);
}

__attribute_used__ bool app_set_binding(bool is_gamepad, gbmulator_joypad_t joypad, unsigned int key, gbmulator_joypad_t *swapped_joypad, unsigned int *swapped_key) {
    if (joypad < 0 || joypad >= GBMULATOR_JOYPAD_END || (!is_gamepad && app.config.keycode_filter && !app.config.keycode_filter(key)))
        return false;

    if (swapped_joypad)
        *swapped_joypad = joypad;
    if (swapped_key)
        *swapped_key = key;

    unsigned int *bindings = is_gamepad ? app.config.gamepad_bindings : app.config.keybindings;

    // detect if the key is already attributed, if yes, swap them
    for (gbmulator_joypad_t i = 0; i < GBMULATOR_JOYPAD_END; i++) {
        if (bindings[i] == key && joypad != i) {
            if (swapped_joypad)
                *swapped_joypad = i;
            if (swapped_key)
                *swapped_key = bindings[i];

            bindings[i] = bindings[joypad];
            break;
        }
    }

    bindings[joypad] = key;

    return true;
}

// TODO link cable (therefore also printer) not working anymore --> git bisect: maybe gb bit shifter is wrong
static void on_printer_new_line_cb(const uint8_t *pixels, size_t current_height, size_t total_height) {
    if (!app.printer || !app.printer_renderer)
        return;

    if (current_height != app.printer_height) {
        app.printer_height = current_height;

        glrenderer_resize_screen(app.printer_renderer, GBPRINTER_IMG_WIDTH, app.printer_height);
    }

    glrenderer_update_screen(app.printer_renderer, pixels);

    if (app.on_printer_new_line)
        app.on_printer_new_line(current_height, total_height);
}

__attribute_used__ bool app_connect_printer(printer_new_line_cb_t on_new_line) {
    if (!app.emu || app.printer)
        return false;

    if (app.linked_emu)
        todo("disconnect linked emu");

    gbmulator_options_t opts = {
        .mode        = GBMULATOR_MODE_GBPRINTER,
        .on_new_line = on_printer_new_line_cb
    };
    app.printer = gbmulator_init(&opts);

    gbmulator_link_connect(app.emu, app.printer, GBMULATOR_LINK_CABLE);

    app.printer_height = 1;
    if (!app.printer_renderer)
        app.printer_renderer = glrenderer_init(GBPRINTER_IMG_WIDTH, app.printer_height, 0);

    app.on_printer_new_line = on_new_line;

    return true;
}

__attribute_used__ void app_printer_disconnect(void) {
    if (!app.emu)
        return;

    gbmulator_link_disconnect(app.emu, GBMULATOR_LINK_CABLE);
    gbmulator_quit(app.printer);
    app.printer = NULL;
}

__attribute_used__ void app_printer_render(void) {
    if (app.printer_renderer)
        glrenderer_render(app.printer_renderer);
}

__attribute_used__ bool app_printer_reset(void) {
    if (!app.printer_renderer)
        return false;

    glrenderer_resize_screen(app.printer_renderer, GBPRINTER_IMG_WIDTH, 1);
    app.printer_height = 1;

    void *pixels = xcalloc(1, GBPRINTER_IMG_WIDTH * app.printer_height * 4);
    glrenderer_update_screen(app.printer_renderer, pixels);
    free(pixels);

    if (app.printer)
        todo("gbprinter_clear_image(app.printer)");

    return true;
}

__attribute_used__ bool app_has_camera(void) {
    if (!app.emu)
        return false;

    return gbmulator_has_peripheral(app.emu, GBMULATOR_PERIPHERAL_CAMERA);
}

__attribute_used__ void app_link_set_host(const char *host) {
    if (!host)
        return;

    snprintf(app.config.link_host, sizeof(app.config.link_host), "%s", host);
}

__attribute_used__ void app_link_set_port(uint16_t port) {
    snprintf(app.config.link_port, sizeof(app.config.link_port), "%u", port);
}

__attribute_used__ bool app_link_start(bool is_server) {
    if (app.sfd >= 0 || !app.emu || app.linked_emu)
        return false;

    if (is_server)
        app.sfd = link_start_server(app.config.link_port);
    else
        app.sfd = link_connect_to_server(app.config.link_host, app.config.link_port);

    gbmulator_t *new_linked_emu;
    if (app.sfd > 0 && link_init_transfer(app.sfd, app.emu, &new_linked_emu)) {
        app.linked_emu = new_linked_emu;
        set_steps_per_frame();

        gbmulator_set_apu_speed(app.emu, 1.0f);
    } else {
        app.sfd = -1; // closed by link_init_transfer in case of error
    }

    return true;
}

__attribute_used__ void app_link_disconnect(void) {
    if (app.sfd < 0)
        return;

    link_cancel();
    close(app.sfd);
    app.sfd = -1;

    if (app.linked_emu) {
        gbmulator_quit(app.linked_emu);
        app.linked_emu = NULL;
    }
}
