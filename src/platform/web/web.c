#include <stdlib.h>
#include <stdio.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/key_codes.h>

#include "../common/glrenderer.h"
#include "../common/alrenderer.h"
#include "../common/config.h"
#include "../common/utils.h"
#include "../../core/core.h"
#include "base64.h"

static int keycode_filter(unsigned int key);

// config struct initialized to defaults
static config_t config = {
    .mode = GB_MODE_CGB,
    .color_palette = PPU_COLOR_PALETTE_ORIG,
    .scale = 2,
    .sound = 0.25f,
    .speed = 1.0f,

    .keybindings = {
        DOM_VK_RIGHT,
        DOM_VK_LEFT,
        DOM_VK_UP,
        DOM_VK_DOWN,
        DOM_VK_NUMPAD0,
        DOM_VK_PERIOD,
        DOM_VK_NUMPAD2,
        DOM_VK_NUMPAD1
    },
    .keycode_filter = (keycode_filter_t) keycode_filter,
    // TODO uncomment both lines below to save/load config for keybindings (emscripten_dom_pk_code_to_string and
    //      emscripten_compute_dom_pk_code are unsuitable because they use PK codes but we use VK codes)
    // .keycode_parser = (keycode_parser_t) emscripten_dom_pk_code_to_string,
    // .keyname_parser = (keyname_parser_t) emscripten_compute_dom_pk_code
};

static bool is_paused = 1;

static glrenderer_t *renderer;

static uint8_t scale;

static int editing_keybind = -1;

static char window_title[sizeof(EMULATOR_NAME) + 19];

static uint8_t joypad_state = 0xFF;

static gb_t *gb;

static int keycode_filter(unsigned int key) {
    switch (key) {
    case DOM_VK_RETURN: case DOM_VK_ENTER:
    case DOM_VK_DELETE: case DOM_VK_BACK_SPACE:
    case DOM_VK_PAUSE: case DOM_VK_ESCAPE:
    case DOM_VK_F1: case DOM_VK_F2:
    case DOM_VK_F3: case DOM_VK_F4:
    case DOM_VK_F5: case DOM_VK_F6:
    case DOM_VK_F7: case DOM_VK_F8:
        return 0;
    default:
        return 1;
    }
}

static void ppu_vblank_cb(const uint8_t *pixels) {
    glrenderer_update_texture(renderer, 0, 0, GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT, pixels);
}

// TODO maybe use same loop as desktop platform
static void loop(void) {
    if (is_paused) {
        emscripten_cancel_main_loop();
    }

    gb_set_joypad_state(gb, joypad_state);

    // run the emulator for the approximate number of steps it takes for the ppu to render a frame
    gb_run_steps(gb, GB_CPU_STEPS_PER_FRAME * config.speed);

    glrenderer_render(renderer);
}

EMSCRIPTEN_KEEPALIVE void set_pause(uint8_t value) {
    if (!gb) return;

    is_paused = value;

    if (is_paused)
        emscripten_cancel_main_loop();
    else
        emscripten_set_main_loop(loop, GB_FRAMES_PER_SECOND, 0);

    EM_ASM({
        setMenuVisibility($0);
    }, !is_paused);
}

static uint8_t *local_storage_get_item(const char *key, size_t *data_length, uint8_t decode) {
    unsigned char *data = (unsigned char *) EM_ASM_INT({
        var item = localStorage.getItem(UTF8ToString($0).replaceAll(' ', '_'));
        if (item === null)
            return null;
        var itemLength = lengthBytesUTF8(item) + 1;

        var ret = _malloc(itemLength);
        stringToUTF8(item, ret, itemLength);
        return ret;
    }, key);

    if (!data) {
        if (data_length) *data_length = 0;
        return NULL;
    }

    if (!decode) {
        if (data_length) *data_length = strlen((char *) data);
        return data;
    }

    size_t out_len;
    unsigned char *decoded_data = base64_decode(data, strlen((char *) data), &out_len);
    if (data_length) *data_length = out_len;
    free(data);
    return decoded_data;
}

static void local_storage_set_item(const char *key, const uint8_t *data, uint8_t encode, size_t len) {
    if (encode) {
        uint8_t *encoded_data = base64_encode(data, len, NULL);
        EM_ASM({
            localStorage.setItem(UTF8ToString($0).replaceAll(' ', '_'), UTF8ToString($1));
        }, key, encoded_data);
        free(encoded_data);
    } else {
        EM_ASM({
            localStorage.setItem(UTF8ToString($0).replaceAll(' ', '_'), UTF8ToString($1));
        }, key, data);
    }
}

void load_config(char *path) {
    uint8_t *data = local_storage_get_item(path, NULL, 0);
    if (!data)
        return;
    config_load_from_string(&config, (const char *) data);
    free(data);
}

static void save_config(const char *path) {
    char *config_buf = config_save_to_string(&config);
    local_storage_set_item(path, (uint8_t *) config_buf, 0, strlen(config_buf));
    free(config_buf);
}

static void save(void) {
    size_t save_length;
    uint8_t *save_data = gb_get_save(gb, &save_length);
    if (!save_data) return;
    local_storage_set_item(gb_get_rom_title(gb), save_data, 1, save_length);
    free(save_data);
}

void load_cartridge(const uint8_t *rom, size_t rom_size) {
    gb_options_t opts = {
        .mode = config.mode,
        .on_new_sample = alrenderer_queue_sample,
        .on_new_frame = ppu_vblank_cb,
        .apu_speed = config.speed,
        .apu_sampling_rate = alrenderer_get_sampling_rate(),
        .palette = config.color_palette
    };
    gb_t *new_emu = gb_init(rom, rom_size, &opts);
    if (!new_emu) return;

    if (gb) {
        save();
        gb_quit(gb);
    }
    gb = new_emu;
    char *rom_title = gb_get_rom_title(gb);
    alrenderer_clear_queue();

    size_t save_length;
    unsigned char *save = local_storage_get_item(rom_title, &save_length, 1);
    if (save) {
        gb_load_save(gb, save, save_length);
        free(save);
    }

    snprintf(window_title, sizeof(window_title), EMULATOR_NAME" - %s", rom_title);
    emscripten_set_window_title(window_title);

    set_pause(0);

    EM_ASM({
        setTheme($0);
    }, config.mode);
}

EMSCRIPTEN_KEEPALIVE void on_before_unload(void) {
    if (gb) {
        save();
        gb_quit(gb);
    }

    // save config
    save_config("config");

    alrenderer_quit();
    glrenderer_quit(renderer);
}

EMSCRIPTEN_KEEPALIVE void on_gui_button_down(gb_joypad_button_t button) {
    if (!is_paused)
        RESET_BIT(joypad_state, button);
}

EMSCRIPTEN_KEEPALIVE void on_gui_button_up(gb_joypad_button_t button) {
    if (!is_paused)
        SET_BIT(joypad_state, button);
}

EMSCRIPTEN_KEEPALIVE void receive_rom(uint8_t *rom, size_t rom_size) {
    load_cartridge(rom, rom_size);
    free(rom);
}

EMSCRIPTEN_KEEPALIVE void reset_rom(void) {
    if (!gb) return;
    gb_reset(gb, config.mode);
    alrenderer_clear_queue();
    set_pause(0);
    EM_ASM({
        setTheme($0);
    }, config.mode);
}

EMSCRIPTEN_KEEPALIVE void set_scale(uint8_t value) {
    config.scale = value;
    if (scale != config.scale) {
        scale = config.scale;
        emscripten_set_canvas_element_size("#canvas", GB_SCREEN_WIDTH * scale, GB_SCREEN_HEIGHT * scale);
        glrenderer_resize_viewport(renderer, GB_SCREEN_WIDTH * scale, GB_SCREEN_HEIGHT * scale);
    }
}

EMSCRIPTEN_KEEPALIVE void set_speed(float value) {
    config.speed = value;
    if (gb)
        gb_set_apu_speed(gb, config.speed);
}

EMSCRIPTEN_KEEPALIVE void set_sound(float value) {
    config.sound = value;
    alrenderer_set_level(config.sound);
}

EMSCRIPTEN_KEEPALIVE void set_color(uint8_t value) {
    config.color_palette = value;
    if (gb)
        gb_set_palette(gb, config.color_palette);
}

EMSCRIPTEN_KEEPALIVE void set_mode(float value) {
    config.mode = value;
    if (!gb) {
        EM_ASM({
            setTheme($0);
        }, config.mode);
    }
}

EMSCRIPTEN_KEEPALIVE void edit_keybind(uint8_t value) {
    editing_keybind = value;
}

bool handle_keyboard_input(int eventType, const EmscriptenKeyboardEvent *e, void *userData) {
    size_t len;
    char *savestate_path;
    char *rom_title;
    gb_joypad_button_t joypad;

    switch (eventType) {
    case EMSCRIPTEN_EVENT_KEYUP:
        if (!gb || is_paused) return 0;

        joypad = keycode_to_joypad(&config, e->keyCode);
        if (joypad < 0) return 1;
        SET_BIT(joypad_state, joypad);
        return 1;
    case EMSCRIPTEN_EVENT_KEYDOWN:
        if (is_paused) {
            if (e->keyCode == DOM_VK_ESCAPE || e->keyCode == DOM_VK_PAUSE) {
                set_pause(0);
                if (editing_keybind >= JOYPAD_RIGHT && editing_keybind <= JOYPAD_START) {
                    EM_ASM({
                        toggleEditingKeybind($0);
                        editingKeybind = -1;
                    }, editing_keybind);
                    editing_keybind = -1;
                }
            } else if (editing_keybind >= JOYPAD_RIGHT && editing_keybind <= JOYPAD_START && config.keycode_filter(e->keyCode)) {
                // check if another keybind is already bound to this key and swap them if this is the case
                for (int i = JOYPAD_RIGHT; i <= JOYPAD_START; i++) {
                    if (i != editing_keybind && config.keybindings[i] == e->keyCode) {
                        config.keybindings[i] = config.keybindings[editing_keybind];
                        EM_ASM({
                            document.getElementById("keybind-setter-" + $0).innerHTML = UTF8ToString($1);
                        }, i, emscripten_dom_vk_to_string(config.keybindings[i]));
                        break;
                    }
                }
        
                config.keybindings[editing_keybind] = e->keyCode;
                EM_ASM({
                    document.getElementById("keybind-setter-" + $0).innerHTML = UTF8ToString($1);
                    toggleEditingKeybind($0);
                    editingKeybind = -1;
                }, editing_keybind, emscripten_dom_vk_to_string(config.keybindings[editing_keybind]));
                editing_keybind = -1;
            }
            return 1;
        }

        switch (e->keyCode) {
        case DOM_VK_PAUSE:
        case DOM_VK_ESCAPE:
            set_pause(1);
            break;
        case DOM_VK_F1: case DOM_VK_F2:
        case DOM_VK_F3: case DOM_VK_F4:
        case DOM_VK_F5: case DOM_VK_F6:
        case DOM_VK_F7: case DOM_VK_F8:
            if (!gb)
                break;

            rom_title = gb_get_rom_title(gb);
            len = strlen(rom_title);
            savestate_path = xmalloc(len + 10);
            snprintf(savestate_path, len + 9, "%s-state-%d", rom_title, e->keyCode - DOM_VK_F1);
            if (e->shiftKey) {
                size_t savestate_length;
                uint8_t *savestate = gb_get_savestate(gb, &savestate_length, 1);
                local_storage_set_item(savestate_path, savestate, 1, savestate_length);
                free(savestate);
            } else {
                size_t savestate_length;
                uint8_t *savestate = local_storage_get_item(savestate_path, &savestate_length, 1);
                int ret = gb_load_savestate(gb, savestate, savestate_length);
                if (ret > 0) {
                    config.mode = ret;
                    EM_ASM({
                        document.getElementById("mode-setter").value = $4;
                    }, config.mode);
                }
                free(savestate);
            }
            free(savestate_path);
            break;
        }

        joypad = keycode_to_joypad(&config, e->keyCode);
        if (joypad < 0) return 1;
        RESET_BIT(joypad_state, joypad);
        return 1;
    default:
        return 0;
    }
}

int main(int argc, char **argv) {
    // load_config() must be called before gb_init()
    // config.keybindings[JOYPAD_B] = DOM_VK_PERIOD; // change default B key to DOM_VK_PERIOD as DOM_VK_KP_PERIOD doesn't work for web
    load_config("config");
    gb = NULL;

    EM_ASM({
        document.getElementById("speed-slider").value = $0;
        document.getElementById("speed-label").innerHTML = "x" + $0.toFixed(1);
        document.getElementById("sound-slider").value = $1;
        document.getElementById("sound-label").innerHTML = $1 + "%";
        document.getElementById("scale-setter").value = $2;
        document.getElementById("color-setter").value = $3;
        document.getElementById("mode-setter").value = $4;
    }, config.speed, config.sound * 100, config.scale, config.color_palette, config.mode);

    for (int i = JOYPAD_RIGHT; i <= JOYPAD_START; i++) {
        EM_ASM({
            document.getElementById("keybind-setter-" + $0).innerHTML = UTF8ToString($1);
        }, i, emscripten_dom_vk_to_string(config.keybindings[i]));
    }

    scale = config.scale;

    emscripten_set_canvas_element_size("#canvas", GB_SCREEN_WIDTH * scale, GB_SCREEN_HEIGHT * scale);

    EmscriptenWebGLContextAttributes attr;
    emscripten_webgl_init_context_attributes(&attr);

    attr.majorVersion = 2;
    attr.minorVersion = 0;

    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = emscripten_webgl_create_context("#canvas", &attr);
    if (!ctx) {
        printf("ERROR: Failed to create WebGL context!\n");
        return EXIT_FAILURE;
    }

    if (emscripten_webgl_make_context_current(ctx) != EMSCRIPTEN_RESULT_SUCCESS) {
        printf("ERROR: Failed to make WebGL context current!\n");
        return EXIT_FAILURE;
    }

    renderer = glrenderer_init(GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT, NULL);
    emscripten_set_window_title(EMULATOR_NAME);

    alrenderer_init(0);

    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, 0, 1, handle_keyboard_input);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, 0, 1, handle_keyboard_input);

    return EXIT_SUCCESS;
}
