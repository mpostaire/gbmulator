#include <stdio.h>
#include <unistd.h>

#include "app.h"
#include "utils.h"
#include "link.h"
#include "bmp.h"
#include "glrenderer.h"
#include "alrenderer.h"

#include "../../core/core.h"

#ifndef __attribute_used__
#define __attribute_used__ __attribute__((__used__))
#endif

#define MAX_TOUCHES 32

static struct {
    bool          is_paused;
    bool          is_rewinding;
    uint32_t      steps_per_frame;
    glrenderer_t *renderer;
    gbmulator_t  *emu;
    config_t      config;

    uint8_t *rom;

    struct {
        gbmulator_t *emu;
        int          sfd;
    } link;

    struct {
        uint16_t            joypad_state;
        uint8_t             joypad_touch_counter[GBMULATOR_JOYPAD_END];
        bool                joypad_key_press_counter[GBMULATOR_JOYPAD_END];
        uint8_t             link_touch_counter;
        glrenderer_obj_id_t touches_current_obj[32];
    } input;

    struct {
        gbmulator_t          *emu;
        glrenderer_t         *renderer;
        printer_new_line_cb_t on_new_line;
    } printer;

    struct {
        uint8_t *data;
        int      width;
        int      height;
        int      row_stride;
        int      rotation;
    } camera;
} app;

static void set_steps_per_frame(void) {
    float speed = app.link.emu ? 1.0f : app.config.speed;

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
        app.input.joypad_touch_counter[joypad]++;
    else
        app.input.joypad_key_press_counter[joypad] = true;

    if (app.input.joypad_touch_counter[joypad] + app.input.joypad_key_press_counter[joypad] == 1) {
        RESET_BIT(app.input.joypad_state, joypad);
        glrenderer_set_obj_tint(app.renderer, (glrenderer_obj_id_t) joypad, 0.5f);
    }
}

static inline void btn_release(gbmulator_joypad_t joypad, bool is_touch) {
    if (joypad < 0 || joypad >= GBMULATOR_JOYPAD_END)
        return;

    if (is_touch)
        app.input.joypad_touch_counter[joypad]--; // TODO why here?
    else
        app.input.joypad_key_press_counter[joypad] = false;

    if (app.input.joypad_touch_counter[joypad] > 0)
        app.input.joypad_touch_counter[joypad]--; // TODO and here? --> only one of those needed?

    if (app.input.joypad_touch_counter[joypad] + app.input.joypad_key_press_counter[joypad] == 0) {
        SET_BIT(app.input.joypad_state, joypad);
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

    app.input.joypad_state = 0xFF;

    uint32_t screen_w;
    uint32_t screen_h;
    app_get_screen_size(&screen_w, &screen_h);

    for (int i = 0; i < MAX_TOUCHES; i++)
        app.input.touches_current_obj[i] = GLRENDERER_OBJ_ID_SCREEN;

    app.renderer = glrenderer_init(screen_w, screen_h, 0);

    alrenderer_init(0);

    app.link.sfd = -1;
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

    if (app.link.emu) {
        gbmulator_quit(app.link.emu);
        app.link.emu = NULL;
    }

    if (app.printer.emu) {
        gbmulator_quit(app.printer.emu);
        app.printer.emu = NULL;
    }

    if (app.camera.data)
        free(app.camera.data);

    if (app.rom)
        free(app.rom);

    config_save_to_file(&app.config, get_config_path());

    alrenderer_quit();
    glrenderer_quit(app.renderer);
}

__attribute_used__ void app_reset(void) {
    if (!app.emu)
        return;

    char *save_path = get_save_path(gbmulator_get_rom_title(app.emu));

    save_battery_to_file(app.emu, save_path);

    gbmulator_options_t opts = gbmulator_get_options(app.emu);
    opts.mode                = app.config.mode;

    gbmulator_reset(app.emu, &opts);

    load_battery_from_file(app.emu, save_path);

    gbmulator_print_status(app.emu);
    alrenderer_clear_queue();
}

__attribute_used__ void app_run_frame(void) {
    if (app.is_paused)
        return;

    if (app.is_rewinding) {
        gbmulator_rewind(app.emu, -1);
    } else {
        gbmulator_set_joypad_state(app.emu, app.input.joypad_state);

        // TODO async or timeout link_exchange_joypad to avoid blocking the gui
        if (app.link.emu) {
            if (!link_exchange_joypad(app.link.sfd, app.emu, app.link.emu)) {
                app_link_disconnect();
                set_steps_per_frame();
                if (app.config.on_link_disconnected)
                    app.config.on_link_disconnected();
            }
        }

        // TODO test with speed > 1
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
        app.input.link_touch_counter++;

        if (app.input.link_touch_counter == 1)
            glrenderer_set_obj_tint(app.renderer, (glrenderer_obj_id_t) GLRENDERER_OBJ_ID_LINK, 0.5f);
        break;
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
        if (app.input.link_touch_counter > 0)
            app.input.link_touch_counter--;

        if (app.input.link_touch_counter == 0) {
            glrenderer_set_obj_tint(app.renderer, (glrenderer_obj_id_t) GLRENDERER_OBJ_ID_LINK, 0.5f);
            if (app.config.on_link_button_touched)
                app.config.on_link_button_touched(app.config.on_link_button_touched_user_data);
        }
        break;
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

    app.input.touches_current_obj[touch_id] = obj_id;
}

__attribute_used__ void app_touch_release(uint8_t touch_id, uint32_t x, uint32_t y) {
    if (touch_id >= MAX_TOUCHES)
        return;

    btn_touch_release(app.input.touches_current_obj[touch_id]);

    app.input.touches_current_obj[touch_id] = GLRENDERER_OBJ_ID_SCREEN;
}

__attribute_used__ void app_touch_move(uint8_t touch_id, uint32_t x, uint32_t y) {
    if (touch_id >= MAX_TOUCHES)
        return;

    glrenderer_obj_id_t obj_id = glrenderer_get_obj_at_coord(app.renderer, x, y);

    if (obj_id != app.input.touches_current_obj[touch_id]) {
        btn_touch_release(app.input.touches_current_obj[touch_id]);
        btn_touch_press(obj_id);
    }

    app.input.touches_current_obj[touch_id] = obj_id;
}

static bool on_camera_capture_image(uint8_t *pixels) {
    if (!app.camera.data)
        return false;

    fit_image(pixels, app.camera.data, app.camera.width, app.camera.height, app.camera.row_stride, app.camera.rotation);
    return true;
}

static uint8_t *on_pixbuf_request_cb(size_t w, size_t h) {
    return glrenderer_swap_buffers(app.renderer, w, h);
}

// TODO renderer buttons scaled smaller when in GBA mode

__attribute_used__ bool app_load_cartridge(uint8_t *rom, size_t rom_size) {
    if (!app.renderer) {
        free(rom);
        return false;
    }

    if (app.rom)
        free(app.rom);
    app.rom = rom;

    gbmulator_options_t opts = {
        .rom                     = app.rom,
        .rom_size                = rom_size,
        .mode                    = app.config.mode,
        .on_new_sample           = alrenderer_queue_sample,
        .on_pixbuf_request       = on_pixbuf_request_cb,
        .on_camera_capture_image = on_camera_capture_image,
        .apu_speed               = app.config.speed,
        .apu_sampling_rate       = alrenderer_get_sampling_rate(),
        .palette                 = app.config.color_palette
    };

    gbmulator_t *new_emu = gbmulator_init(&opts);
    if (!new_emu) {
        if (opts.mode == GBMULATOR_MODE_GBA)
            opts.mode = GBMULATOR_MODE_GBC;
        else
            opts.mode = GBMULATOR_MODE_GBA;

        new_emu = gbmulator_init(&opts);
        if (!new_emu) {
            free(rom);
            return false;
        }
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

    gbmulator_options_t opts = gbmulator_get_options(app.emu);

    return opts.mode;
}

__attribute_used__ bool app_set_mode(gbmulator_mode_t mode) {
    if (mode < 0 || mode >= GBMULATOR_MODE_GBPRINTER)
        return false;

    app.config.mode = mode;

    return !app.emu;
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

__attribute_used__ void app_set_viewport_size(uint32_t viewport_w, uint32_t viewport_h) {
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
    if (app.emu && !app.link.emu)
        save_state_to_file(app.emu, get_savestate_path(gbmulator_get_rom_title(app.emu), slot));
}

__attribute_used__ void app_load_state(int slot) {
    if (app.emu && !app.link.emu)
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

        if (app.config.on_link_button_touched)
            visible_btns |= 1 << GLRENDERER_OBJ_ID_LINK;

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

static uint8_t *on_printer_pixbuf_request_cb(size_t w, size_t h) {
    glrenderer_resize_viewport(app.printer.renderer, w * 2, h * 2);

    if (app.printer.on_new_line)
        app.printer.on_new_line(h);

    return glrenderer_swap_buffers(app.printer.renderer, w, h);
}

__attribute_used__ bool app_connect_printer(printer_new_line_cb_t on_new_line) {
    if (!app.emu || app.printer.emu)
        return false;

    if (app.link.emu)
        todo("disconnect linked emu");

    gbmulator_options_t opts = {
        .mode              = GBMULATOR_MODE_GBPRINTER,
        .on_pixbuf_request = on_printer_pixbuf_request_cb
    };
    app.printer.emu = gbmulator_init(&opts);

    gbmulator_link_connect(app.emu, app.printer.emu, GBMULATOR_LINK_CABLE);

    app.printer.on_new_line = on_new_line;

    return true;
}

__attribute_used__ void app_printer_disconnect(void) {
    if (!app.emu)
        return;

    gbmulator_link_disconnect(app.emu, GBMULATOR_LINK_CABLE);
    gbmulator_quit(app.printer.emu);
    app.printer.emu = NULL;
}

__attribute_used__ void app_printer_render(void) {
    if (!app.printer.renderer)
        app.printer.renderer = glrenderer_init(GBPRINTER_IMG_WIDTH, 1, 0);

    glrenderer_render(app.printer.renderer);
}

__attribute_used__ void app_printer_set_viewport_size(uint32_t viewport_w, uint32_t viewport_h) {
    glrenderer_resize_viewport(app.printer.renderer, viewport_w, viewport_h);
}

__attribute_used__ bool app_printer_reset(void) {
    if (!app.printer.renderer)
        return false;

    gbmulator_reset(app.printer.emu, NULL);

    return true;
}

__attribute_used__ bool app_printer_save(const char *path) {
    if (!app.printer.emu)
        return false;

    size_t printer_heigth = 0;
    gbmulator_get_save(app.printer.emu, NULL, &printer_heigth);

    if (printer_heigth == 0)
        return false;

    bmp_image_t *image = xmalloc(sizeof(*image) + GBPRINTER_IMG_WIDTH * printer_heigth * 4);
    image->w           = GBPRINTER_IMG_WIDTH;
    image->h           = printer_heigth;

    gbmulator_get_save(app.printer.emu, image->data, &printer_heigth);

    if (printer_heigth == 0) {
        free(image);
        return false;
    }

    size_t   data_len = 0;
    uint8_t *data     = bmp_encode(image, &data_len);
    free(image);

    bool ret = data && write_file(path, data, data_len);

    if (data)
        free(data);

    return ret;
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
    if (app.link.sfd >= 0 || !app.emu || app.link.emu)
        return false;

    if (is_server)
        app.link.sfd = link_start_server(app.config.link_port);
    else
        app.link.sfd = link_connect_to_server(app.config.link_host, app.config.link_port);

    gbmulator_t *new_linked_emu;
    if (app.link.sfd >= 0 && link_init_transfer(app.link.sfd, app.emu, &new_linked_emu)) {
        app.link.emu = new_linked_emu;
        set_steps_per_frame();

        gbmulator_set_apu_speed(app.emu, 1.0f);
        return true;
    } else {
        app.link.sfd = -1; // closed by link_init_transfer in case of error
        return false;
    }
}

__attribute_used__ void app_link_disconnect(void) {
    if (app.link.sfd < 0)
        return;

    link_cancel();
    close(app.link.sfd);
    app.link.sfd = -1;

    if (app.link.emu) {
        gbmulator_options_t opts        = gbmulator_get_options(app.emu);
        gbmulator_options_t linked_opts = gbmulator_get_options(app.link.emu);

        if (opts.rom != linked_opts.rom)
            free(linked_opts.rom);

        gbmulator_quit(app.link.emu);
        app.link.emu = NULL;
    }
}

__attribute_used__ void app_update_camera_buffer(uint8_t *data, int width, int height, int row_stride, int rotation) {
    size_t sz = width * height * sizeof(*app.camera.data);
    if (!app.camera.data)
        app.camera.data = xmalloc(sz);

    memcpy(app.camera.data, data, sz);

    app.camera.width      = width;
    app.camera.height     = height;
    app.camera.row_stride = row_stride;
    app.camera.rotation   = rotation;
}
