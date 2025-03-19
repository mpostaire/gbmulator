#include <stdlib.h>
#include <stdio.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

#include "../common/glrenderer.h"
#include "../common/alrenderer.h"
#include "../common/config.h"
#include "../common/utils.h"
#include "../../core/core.h"
#include "base64.h"

// static int keycode_filter(SDL_Keycode key);

// config struct initialized to defaults
static config_t config = {
    .mode = GB_MODE_CGB,
    .color_palette = PPU_COLOR_PALETTE_ORIG,
    .scale = 2,
    .sound = 0.25f,
    .speed = 1.0f,
    .link_host = "127.0.0.1",
    .link_port = "7777",

    .gamepad_bindings = {
        // SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
        // SDL_CONTROLLER_BUTTON_DPAD_LEFT,
        // SDL_CONTROLLER_BUTTON_DPAD_UP,
        // SDL_CONTROLLER_BUTTON_DPAD_DOWN,
        // SDL_CONTROLLER_BUTTON_A,
        // SDL_CONTROLLER_BUTTON_B,
        // SDL_CONTROLLER_BUTTON_BACK,
        // SDL_CONTROLLER_BUTTON_START
    },

    .keybindings = {
        // SDLK_RIGHT,
        // SDLK_LEFT,
        // SDLK_UP,
        // SDLK_DOWN,
        // SDLK_KP_0,
        // SDLK_KP_PERIOD,
        // SDLK_KP_2,
        // SDLK_KP_1
    },
    // .keycode_filter = (keycode_filter_t) keycode_filter,
    // .keycode_parser = (keycode_parser_t) SDL_GetKeyName,
    // .keyname_parser = (keyname_parser_t) SDL_GetKeyFromName
};

static bool is_paused = 1;

static glrenderer_t *renderer;

// static SDL_GameController *pad;
// static bool is_controller_present = 0;

static uint8_t scale;

static int editing_keybind = -1;

static char window_title[sizeof(EMULATOR_NAME) + 19];

static gb_t *gb;

// static int keycode_filter(SDL_Keycode key) {
//     switch (key) {
//     case SDLK_RETURN: case SDLK_KP_ENTER:
//     case SDLK_DELETE: case SDLK_BACKSPACE:
//     case SDLK_PAUSE: case SDLK_ESCAPE:
//     case SDLK_F1: case SDLK_F2:
//     case SDLK_F3: case SDLK_F4:
//     case SDLK_F5: case SDLK_F6:
//     case SDLK_F7: case SDLK_F8:
//         return 0;
//     default:
//         return 1;
//     }
// }

static void ppu_vblank_cb(const uint8_t *pixels) {
    glrenderer_update_texture(renderer, 0, 0, GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT, pixels);
}

// TODO maybe use same loop as desktop platform
static void loop(void) {
    if (is_paused) {
        emscripten_cancel_main_loop();
        // emscripten_set_main_loop(paused_loop, 30, 1);
    }

    glrenderer_render(renderer);
    // handle_input(); // keep this the closest possible before gb_run_steps() to reduce input inaccuracies

    // run the emulator for the approximate number of steps it takes for the ppu to render a frame
    gb_run_steps(gb, GB_CPU_STEPS_PER_FRAME * config.speed);
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
        .apu_sound_level = config.sound,
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
        document.getElementById('reset-rom').disabled = 0;
        document.getElementById('mode-setter-label').innerHTML = "Mode: (applied on restart)";
        var x = document.getElementById('open-menu');
        x.disabled = 0;
        x.classList.toggle("paused");
    }, config.mode);
}

EMSCRIPTEN_KEEPALIVE void on_before_unload(void) {
    if (gb) {
        save();
        gb_quit(gb);
    }

    // save config
    save_config("config");

    // if (is_controller_present)
    //     SDL_GameControllerClose(pad);

    alrenderer_quit();
    glrenderer_quit(renderer);
}

EMSCRIPTEN_KEEPALIVE void on_gui_button_down(gb_joypad_button_t button) {
    if (!is_paused)
        gb_joypad_press(gb, button);
}

EMSCRIPTEN_KEEPALIVE void on_gui_button_up(gb_joypad_button_t button) {
    if (!is_paused)
        gb_joypad_release(gb, button);
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
    }
}

EMSCRIPTEN_KEEPALIVE void set_speed(float value) {
    config.speed = value;
    if (gb)
        gb_set_apu_speed(gb, config.speed);
}

EMSCRIPTEN_KEEPALIVE void set_sound(float value) {
    config.sound = value;
    if (gb)
        gb_set_apu_sound_level(gb, config.sound);
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

EMSCRIPTEN_KEEPALIVE void *_xmalloc(size_t size) {
    return xmalloc(size);
}

static void handle_input(void) {
    // SDL_Event event;
    // size_t len;
    // char *savestate_path;
    // char *rom_title;
    // while (SDL_PollEvent(&event)) {
    //     switch (event.type) {
    //     case SDL_KEYDOWN:
    //         if (event.key.repeat)
    //             break;
    //         if (is_paused) {
    //             if (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_PAUSE) {
    //                 set_pause(0);
    //                 if (editing_keybind >= JOYPAD_RIGHT && editing_keybind <= JOYPAD_START) {
    //                     EM_ASM({
    //                         toggleEditingKeybind($0);
    //                         editingKeybind = -1;
    //                     }, editing_keybind);
    //                     editing_keybind = -1;
    //                 }
    //             } else if (editing_keybind >= JOYPAD_RIGHT && editing_keybind <= JOYPAD_START && config.keycode_filter(event.key.keysym.sym)) {
    //                 // check if another keybind is already bound to this key and swap them if this is the case
    //                 for (int i = JOYPAD_RIGHT; i <= JOYPAD_START; i++) {
    //                     if (i != editing_keybind && (int) config.keybindings[i] == event.key.keysym.sym) {
    //                         config.keybindings[i] = config.keybindings[editing_keybind];
    //                         EM_ASM({
    //                             document.getElementById("keybind-setter-" + $0).innerHTML = UTF8ToString($1);
    //                         }, i, SDL_GetKeyName(config.keybindings[i]));
    //                         break;
    //                     }
    //                 }

    //                 config.keybindings[editing_keybind] = event.key.keysym.sym;
    //                 EM_ASM({
    //                     document.getElementById("keybind-setter-" + $0).innerHTML = UTF8ToString($1);
    //                     toggleEditingKeybind($0);
    //                     editingKeybind = -1;
    //                 }, editing_keybind, SDL_GetKeyName(config.keybindings[editing_keybind]));
    //                 editing_keybind = -1;
    //             }
    //             break;
    //         }
    //         switch (event.key.keysym.sym) {
    //         case SDLK_PAUSE:
    //         case SDLK_ESCAPE:
    //             set_pause(1);
    //             break;
    //         case SDLK_F1: case SDLK_F2:
    //         case SDLK_F3: case SDLK_F4:
    //         case SDLK_F5: case SDLK_F6:
    //         case SDLK_F7: case SDLK_F8:
    //             if (!gb)
    //                 break;
    //             rom_title = gb_get_rom_title(gb);
    //             len = strlen(rom_title);
    //             savestate_path = xmalloc(len + 10);
    //             snprintf(savestate_path, len + 9, "%s-state-%d", rom_title, event.key.keysym.sym - SDLK_F1);
    //             if (event.key.keysym.mod & KMOD_SHIFT) {
    //                 size_t savestate_length;
    //                 uint8_t *savestate = gb_get_savestate(gb, &savestate_length, 1);
    //                 local_storage_set_item(savestate_path, savestate, 1, savestate_length);
    //                 free(savestate);
    //             } else {
    //                 size_t savestate_length;
    //                 uint8_t *savestate = local_storage_get_item(savestate_path, &savestate_length, 1);
    //                 int ret = gb_load_savestate(gb, savestate, savestate_length);
    //                 if (ret > 0) {
    //                     config.mode = ret;
    //                     EM_ASM({
    //                         document.getElementById("mode-setter").value = $4;
    //                     }, config.mode);
    //                 }
    //                 free(savestate);
    //             }
    //             free(savestate_path);
    //             break;
    //         }
    //         if (!is_paused)
    //             gb_joypad_press(gb, keycode_to_joypad(&config, event.key.keysym.sym));
    //         break;
    //     case SDL_KEYUP:
    //         if (!event.key.repeat && !is_paused)
    //             gb_joypad_release(gb, keycode_to_joypad(&config, event.key.keysym.sym));
    //         break;
    //     case SDL_CONTROLLERBUTTONDOWN:
    //         if (event.cbutton.button == SDL_CONTROLLER_BUTTON_GUIDE) {
    //             set_pause(!is_paused);
    //             if (editing_keybind >= 0) {
    //                 EM_ASM({
    //                     toggleEditingKeybind($0);
    //                     editingKeybind = -1;
    //                 }, editing_keybind);
    //                 editing_keybind = -1;
    //             }
    //             break;
    //         }
    //         if (!is_paused)
    //             gb_joypad_press(gb, button_to_joypad(&config, event.cbutton.button));
    //         break;
    //     case SDL_CONTROLLERBUTTONUP:
    //         if (!is_paused)
    //             gb_joypad_release(gb, button_to_joypad(&config, event.cbutton.button));
    //         break;
    //     case SDL_CONTROLLERDEVICEADDED:
    //         if (!is_controller_present) {
    //             pad = SDL_GameControllerOpen(event.cdevice.which);
    //             is_controller_present = 1;
    //             printf("%s connected\n", SDL_GameControllerName(pad));
    //         }
    //         break;
    //     case SDL_CONTROLLERDEVICEREMOVED:
    //         if (is_controller_present) {
    //             SDL_GameControllerClose(pad);
    //             is_controller_present = 0;
    //             printf("%s disconnected\n", SDL_GameControllerName(pad));
    //         }
    //         break;
    //     }
    // }
}

int main(int argc, char **argv) {
    // load_config() must be called before gb_init()
    // config.keybindings[JOYPAD_B] = SDLK_PERIOD; // change default B key to SDLK_PERIOD as SDLK_KP_PERIOD doesn't work for web
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

    // separate calls necessary for SDL_GetKeyName()
    for (int i = JOYPAD_RIGHT; i <= JOYPAD_START; i++) {
        // EM_ASM({
        //     document.getElementById("keybind-setter-" + $0).innerHTML = UTF8ToString($1);
        // }, i, SDL_GetKeyName(config.keybindings[i]));
    }

    scale = config.scale;

    // SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER);

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

    return EXIT_SUCCESS;
}
