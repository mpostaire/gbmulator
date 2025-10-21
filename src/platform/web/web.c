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
    .keycode_filter = keycode_filter,
};

static uint8_t viewport_scale = 2;

static bool keycode_filter(unsigned int key) {
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

EMSCRIPTEN_KEEPALIVE void set_pause(uint8_t value) {
    app_set_pause(value);

    if (value)
        emscripten_cancel_main_loop();
    else
        emscripten_set_main_loop(app_loop, GB_FRAMES_PER_SECOND, 0);
}

EMSCRIPTEN_KEEPALIVE void set_scale(uint8_t value) {
    viewport_scale = value;

    uint32_t screen_w;
    uint32_t screen_h;

    config_t config;
    app_get_config(&config);

    switch (config.mode) {
    case GBMULATOR_MODE_GB:
    case GBMULATOR_MODE_GBC:
        screen_w = GB_SCREEN_WIDTH;
        screen_h = GB_SCREEN_HEIGHT;
        break;
    case GBMULATOR_MODE_GBA:
        screen_w = GBA_SCREEN_WIDTH;
        screen_h = GBA_SCREEN_HEIGHT;
        break;
    default:
        eprintf("%d is not a valid mode\n", config.mode);
        return;
    }

    uint32_t viewport_w = screen_w * viewport_scale;
    uint32_t viewport_h = screen_h * viewport_scale;

    emscripten_set_canvas_element_size("#canvas", viewport_w, viewport_h);
    app_set_size(screen_w, screen_h, viewport_w, viewport_h);
}

bool handle_keyboard_input(int eventType, const EmscriptenKeyboardEvent *e, void *userData) {
    size_t len;
    char *savestate_path;
    char *rom_title;
    gbmulator_joypad_button_t joypad;

    switch (eventType) {
    case EMSCRIPTEN_EVENT_KEYUP:
        joypad = app_keycode_to_joypad(e->keyCode);
        if (joypad < 0) return true;
        app_joypad_release(joypad);
        return true;
    case EMSCRIPTEN_EVENT_KEYDOWN:
        // if (is_paused) {
        //     if (e->keyCode == DOM_VK_ESCAPE || e->keyCode == DOM_VK_PAUSE)
        //         set_pause(0);
        //     return true;
        // }

        switch (e->keyCode) {
        case DOM_VK_PAUSE:
        case DOM_VK_ESCAPE:
            set_pause(1);
            break;
        case DOM_VK_F1: case DOM_VK_F2:
        case DOM_VK_F3: case DOM_VK_F4:
        case DOM_VK_F5: case DOM_VK_F6:
        case DOM_VK_F7: case DOM_VK_F8:
            // if (!emu)
            //     break;

            // rom_title = gbmulator_get_rom_title(emu);
            // len = strlen(rom_title);
            // savestate_path = xmalloc(len + 10);
            // snprintf(savestate_path, len + 9, "%s-state-%d", rom_title, e->keyCode - DOM_VK_F1);
            // if (e->shiftKey) {
            //     size_t savestate_length;
            //     uint8_t *savestate = gbmulator_get_savestate(emu, &savestate_length, 1);
            //     local_storage_set_item(savestate_path, savestate, true, savestate_length);
            //     free(savestate);
            // } else {
            //     size_t savestate_length;
            //     uint8_t *savestate = local_storage_get_item(savestate_path, &savestate_length, 1);
            //     int ret = gbmulator_load_savestate(emu, savestate, savestate_length);
            //     if (ret > 0) {
            //         // gbmulator_options_t opts;
            //         // gb_get_options(emu, &opts);
            //         // config.mode = opts.mode;
            //         // EM_ASM({
            //         //     document.getElementById("mode-setter").value = $4;
            //         // }, config.mode);
            //     }
            //     free(savestate);
            // }
            // free(savestate_path);
            break;
        }

        joypad = app_keycode_to_joypad(e->keyCode);
        if (joypad < 0) return true;
        app_joypad_press(joypad);
        return true;
    default:
        return false;
    }
}

int main(int argc, char **argv) {
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

    app_init(&default_config);

    config_t config;
    app_get_config(&config);

    EM_ASM({
        document.getElementById("speed-slider").value = $0;
        document.getElementById("speed-label").innerHTML = "x" + $0.toFixed(1);
        document.getElementById("sound-slider").value = $1;
        document.getElementById("sound-label").innerHTML = $1 + "%";
        document.getElementById("scale-setter").value = $2;
        document.getElementById("color-setter").value = $3;
        document.getElementById("mode-setter").value = $4;
    }, config.speed, config.sound * 100, viewport_scale, config.color_palette, config.mode);

    for (int i = 0; i <= JOYPAD_L; i++) {
        EM_ASM({
            elem = document.getElementById("keybind-info-" + $0);
            if (elem)
                elem.innerHTML = UTF8ToString($1);
        }, i, emscripten_dom_vk_to_string(config.keybindings[i]));
    }

    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, 0, 1, handle_keyboard_input);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, 0, 1, handle_keyboard_input);

    set_scale(viewport_scale);

    char *config_dir = get_config_dir();
    char *save_dir = get_save_dir();
    char *savestate_dir = get_savestate_dir();

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

        FS.syncfs(true, function (err) {
            console.log("IDBFS syncfs: " + err ? err : "OK")
        });
    }, config_dir, save_dir, savestate_dir);

    return EXIT_SUCCESS;
}
