#include <stdlib.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/key_codes.h>

#include "../common/app.h"
#include "../common/utils.h"

static bool keycode_filter(unsigned int key);

// config struct initialized to defaults
static config_t default_config = {
    .mode          = GBMULATOR_MODE_GBC,
    .color_palette = PPU_COLOR_PALETTE_ORIG,
    .sound         = 0.25f,
    .speed         = 1.0f,
    .sound_drc     = true,

    // clang-format off
    .keybindings = {
        DOM_VK_NUMPAD0,
        DOM_VK_PERIOD,
        DOM_VK_NUMPAD2,
        DOM_VK_NUMPAD1,
        DOM_VK_RIGHT,
        DOM_VK_LEFT,
        DOM_VK_UP,
        DOM_VK_DOWN,
        DOM_VK_NUMPAD5,
        DOM_VK_NUMPAD4
    },
    // clang-format on
    .keycode_filter = keycode_filter,
};

static bool keycode_filter(unsigned int key) {
    return false;
}

EMSCRIPTEN_KEEPALIVE void set_pause(uint8_t value) {
    if (!app_get_rom_title() || app_is_paused() == value)
        return;

    app_set_pause(value);

    if (value)
        emscripten_cancel_main_loop();
    else
        emscripten_set_main_loop(app_loop, app_get_fps(), 0);

    // clang-format off
    EM_ASM({
        setMenuVisibility($0);
    }, value);
    // clang-format on
}

EMSCRIPTEN_KEEPALIVE void set_scale(uint8_t value) {
    uint32_t viewport_w;
    uint32_t viewport_h;
    app_get_screen_size(&viewport_w, &viewport_h);

    viewport_w *= value;
    viewport_h *= value;

    emscripten_set_canvas_element_size("#canvas", viewport_w, viewport_h);
    app_set_size(viewport_w, viewport_h);
}

bool on_keyboard_input(int eventType, const EmscriptenKeyboardEvent *e, void *userData) {
    switch (eventType) {
    case EMSCRIPTEN_EVENT_KEYUP:
        app_joypad_release(e->keyCode, false);
        return true;
    case EMSCRIPTEN_EVENT_KEYDOWN:
        switch (e->keyCode) {
        case DOM_VK_PAUSE:
        case DOM_VK_ESCAPE:
            set_pause(!app_is_paused());
            return true;
        case DOM_VK_F1:
        case DOM_VK_F2:
        case DOM_VK_F3:
        case DOM_VK_F4:
        case DOM_VK_F5:
        case DOM_VK_F6:
        case DOM_VK_F7:
        case DOM_VK_F8:
            if (e->shiftKey) {
                app_save_state(e->keyCode - DOM_VK_F1);
            } else {
                app_load_state(e->keyCode - DOM_VK_F1);

                // clang-format off
                EM_ASM({
                    document.getElementById("mode-setter").value = $0;
                    setTheme($0);
                    Module.mode = $0;
                }, app_get_mode());
                // clang-format on
            }
            return true;
        case DOM_VK_F11:
            if (app_get_rom_title()) {
                // clang-format off
                EM_ASM(
                    canvas.requestFullscreen();
                );
                // clang-format on
            }
            return true;
        default:
            app_joypad_press(e->keyCode, false);
            return true;
        }
    default:
        return false;
    }
}

EMSCRIPTEN_KEEPALIVE void finish_init(void) {
    // TODO L/R buttons

    app_init(&default_config);

    config_t config;
    app_get_config(&config);

    // clang-format off
    EM_ASM({
        setScale();
        setTheme($3);

        document.getElementById("speed-slider").value = $0;
        document.getElementById("speed-label").innerHTML = "x" + $0.toFixed(1);
        document.getElementById("sound-slider").value = $1;
        document.getElementById("sound-label").innerHTML = $1 + "%";
        document.getElementById("color-setter").value = $2;
        document.getElementById("mode-setter").value = $3;
    }, config.speed, config.sound * 100, config.color_palette, config.mode);
    // clang-format on

    for (int i = 0; i <= JOYPAD_L; i++) {
        // clang-format off
        EM_ASM({
            elem = document.getElementById("keybind-info-" + $0);
            if (elem)
                elem.innerHTML = UTF8ToString($1).replace("DOM_VK_", "");
        }, i, emscripten_dom_vk_to_string(config.keybindings[i]));
        // clang-format on
    }
}

bool on_touch_input(int eventType, const EmscriptenTouchEvent *e, void *userData) {
    bool ret = false;

    for (int i = 0; i < e->numTouches; i++) {
        if (!e->touches[i].isChanged)
            continue;

        switch (eventType) {
        case EMSCRIPTEN_EVENT_TOUCHSTART:
            app_touch_press(e->touches[i].pageX, e->touches[i].pageY);
            ret = true;
            break;
        case EMSCRIPTEN_EVENT_TOUCHMOVE:
            app_touch_move(e->touches[i].pageX, e->touches[i].pageY);
            ret = true;
            break;
        case EMSCRIPTEN_EVENT_TOUCHEND:
        case EMSCRIPTEN_EVENT_TOUCHCANCEL:
            app_touch_release(e->touches[i].pageX, e->touches[i].pageY);
            ret = true;
            break;
        default:
            break;
        }
    }

    return ret;
}

int main(int argc, char **argv) {
    emscripten_set_window_title(EMULATOR_NAME);

    EmscriptenWebGLContextAttributes attr;
    emscripten_webgl_init_context_attributes(&attr);

    attr.majorVersion = 2;
    attr.minorVersion = 0;

    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = emscripten_webgl_create_context("#canvas", &attr);
    if (!ctx) {
        eprintf("ERROR: Failed to create WebGL context!\n");
        return EXIT_FAILURE;
    }

    if (emscripten_webgl_make_context_current(ctx) != EMSCRIPTEN_RESULT_SUCCESS) {
        eprintf("ERROR: Failed to make WebGL context current!\n");
        return EXIT_FAILURE;
    }

    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, 0, 1, on_keyboard_input);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, 0, 1, on_keyboard_input);

    emscripten_set_touchstart_callback("canvas", NULL, true, on_touch_input);
    emscripten_set_touchend_callback("canvas", NULL, true, on_touch_input);
    emscripten_set_touchcancel_callback("canvas", NULL, true, on_touch_input);
    emscripten_set_touchmove_callback("canvas", NULL, true, on_touch_input);

    char *config_dir    = get_config_dir();
    char *save_dir      = get_save_dir();
    char *savestate_dir = get_savestate_dir();

    // clang-format off
    EM_ASM({
        let dir = UTF8ToString($0);
        FS.mkdirTree(dir);
        FS.mount(IDBFS, {}, dir);

        dir = UTF8ToString($1);
        FS.mkdirTree(dir);
        FS.mount(IDBFS, {}, dir);

        dir = UTF8ToString($2);
        FS.mkdirTree(dir);
        FS.mount(IDBFS, {}, dir);

        FS.syncfs(true, err => {
            if (err)
                console.error("IDBFS syncfs error:", err);
            Module.ccall('finish_init');
        });
    }, config_dir, save_dir, savestate_dir);
    // clang-format on

    return EXIT_SUCCESS;
}
