#include <stdlib.h>
#include <stdio.h>
#include <SDL2/SDL.h>
#include <emscripten.h>

#include "../common/config.h"
#include "../common/utils.h"
#include "../../emulator/emulator.h"
#include "base64.h"

static int keycode_filter(SDL_Keycode key);

// config struct initialized to defaults
config_t config = {
    .mode = CGB,
    .color_palette = PPU_COLOR_PALETTE_ORIG,
    .scale = 2,
    .sound = 0.25f,
    .speed = 1.0f,
    .link_host = "127.0.0.1",
    .link_port = "7777",

    .gamepad_bindings = {
        SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
        SDL_CONTROLLER_BUTTON_DPAD_LEFT,
        SDL_CONTROLLER_BUTTON_DPAD_UP,
        SDL_CONTROLLER_BUTTON_DPAD_DOWN,
        SDL_CONTROLLER_BUTTON_A,
        SDL_CONTROLLER_BUTTON_B,
        SDL_CONTROLLER_BUTTON_BACK,
        SDL_CONTROLLER_BUTTON_START
    },

    .keybindings = {
        SDLK_RIGHT,
        SDLK_LEFT,
        SDLK_UP,
        SDLK_DOWN,
        SDLK_KP_0,
        SDLK_KP_PERIOD,
        SDLK_KP_2,
        SDLK_KP_1
    },
    .keycode_filter = (keycode_filter_t) keycode_filter,
    .keycode_parser = (keycode_parser_t) SDL_GetKeyName,
    .keyname_parser = (keyname_parser_t) SDL_GetKeyFromName
};

SDL_bool is_paused = SDL_TRUE;

SDL_Renderer *renderer;
SDL_Window *window;

SDL_Texture *ppu_texture;
int ppu_texture_pitch;

SDL_AudioDeviceID audio_device;

SDL_GameController *pad;
SDL_bool is_controller_present = SDL_FALSE;

byte_t scale;

int editing_keybind = -1;

char window_title[sizeof(EMULATOR_NAME) + 19];

emulator_t *emu;

static int keycode_filter(SDL_Keycode key) {
    switch (key) {
    case SDLK_RETURN: case SDLK_KP_ENTER:
    case SDLK_DELETE: case SDLK_BACKSPACE:
    case SDLK_PAUSE: case SDLK_ESCAPE:
    case SDLK_F1: case SDLK_F2:
    case SDLK_F3: case SDLK_F4:
    case SDLK_F5: case SDLK_F6:
    case SDLK_F7: case SDLK_F8:
        return 0;
    default:
        return 1;
    }
}

static void ppu_vblank_cb(const byte_t *pixels) {
    SDL_UpdateTexture(ppu_texture, NULL, pixels, ppu_texture_pitch);
}

static void apu_samples_ready_cb(const void *audio_buffer, int audio_buffer_size) {
    while (SDL_GetQueuedAudioSize(audio_device) > (unsigned int) (audio_buffer_size * 8))
        emscripten_sleep(1);
    SDL_QueueAudio(audio_device, audio_buffer, audio_buffer_size);
}

EMSCRIPTEN_KEEPALIVE void set_pause(uint8_t value) {
    if (!emu) return;
    is_paused = value;
    EM_ASM({
        setMenuVisibility($0);
    }, !is_paused);
}

static byte_t *local_storage_get_item(const char *key, size_t *data_length, byte_t decode) {
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

static void local_storage_set_item(const char *key, const byte_t *data, byte_t encode, size_t len) {
    if (encode) {
        byte_t *encoded_data = base64_encode(data, len, NULL);
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
    byte_t *data = local_storage_get_item(path, NULL, 0);
    if (!data)
        return;
    config_load_from_string(&config, (const char *) data);
    free(data);
}

static void save_config(const char *path) {
    char *config_buf = config_save_to_string(&config);
    local_storage_set_item(path, (byte_t *) config_buf, 0, strlen(config_buf));
    free(config_buf);
}

static void save(void) {
    size_t save_length;
    byte_t *save_data = emulator_get_save(emu, &save_length);
    if (!save_data) return;
    local_storage_set_item(emulator_get_rom_title(emu), save_data, 1, save_length);
    free(save_data);
}

void load_cartridge(const byte_t *rom_data, size_t rom_size) {
    emulator_options_t opts = {
        .mode = config.mode,
        .on_apu_samples_ready = apu_samples_ready_cb,
        .on_new_frame = ppu_vblank_cb,
        .apu_speed = config.speed,
        .apu_sound_level = config.sound,
        .palette = config.color_palette
    };
    emulator_t *new_emu = emulator_init(rom_data, rom_size, &opts);
    if (!new_emu) return;

    if (emu) {
        save();
        emulator_quit(emu);
    }
    emu = new_emu;
    char *rom_title = emulator_get_rom_title(emu);

    size_t save_length;
    unsigned char *save = local_storage_get_item(rom_title, &save_length, 1);
    if (save) {
        emulator_load_save(emu, save, save_length);
        free(save);
    }

    snprintf(window_title, sizeof(window_title), EMULATOR_NAME" - %s", rom_title);
    SDL_SetWindowTitle(window, window_title);

    set_pause(SDL_FALSE);

    EM_ASM({
        setTheme($0);
        document.getElementById('reset-rom').disabled = false;
        document.getElementById('mode-setter-label').innerHTML = "Mode: (applied on restart)";
        var x = document.getElementById('open-menu');
        x.disabled = false;
        x.classList.toggle("paused");
    }, config.mode);
}

EMSCRIPTEN_KEEPALIVE void on_before_unload(void) {
    if (emu) {
        save();
        emulator_quit(emu);
    }

    // save config
    save_config("config");

    if (is_controller_present)
        SDL_GameControllerClose(pad);

    SDL_CloseAudioDevice(audio_device);

    SDL_DestroyTexture(ppu_texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_Quit();
}

EMSCRIPTEN_KEEPALIVE void on_gui_button_down(joypad_button_t button) {
    if (!is_paused)
        emulator_joypad_press(emu, button);
}

EMSCRIPTEN_KEEPALIVE void on_gui_button_up(joypad_button_t button) {
    if (!is_paused)
        emulator_joypad_release(emu, button);
}

EMSCRIPTEN_KEEPALIVE void receive_rom_data(uint8_t *rom_data, size_t rom_size) {
    load_cartridge(rom_data, rom_size);
    free(rom_data);
}

EMSCRIPTEN_KEEPALIVE void reset_rom(void) {
    if (!emu) return;
    emulator_reset(emu, config.mode);
    set_pause(SDL_FALSE);
    EM_ASM({
        setTheme($0);
    }, config.mode);
}

EMSCRIPTEN_KEEPALIVE void set_scale(uint8_t value) {
    config.scale = value;
    if (scale != config.scale) {
        scale = config.scale;
        SDL_SetWindowSize(window, GB_SCREEN_WIDTH * scale, GB_SCREEN_HEIGHT * scale);
    }
}

EMSCRIPTEN_KEEPALIVE void set_speed(float value) {
    config.speed = value;
    if (emu)
        emulator_set_apu_speed(emu, config.speed);
}

EMSCRIPTEN_KEEPALIVE void set_sound(float value) {
    config.sound = value;
    if (emu)
        emulator_set_apu_sound_level(emu, config.sound);
}

EMSCRIPTEN_KEEPALIVE void set_color(uint8_t value) {
    config.color_palette = value;
    if (emu) {
        emulator_update_pixels_with_palette(emu, config.color_palette);
        emulator_set_palette(emu, config.color_palette);
    }
}

EMSCRIPTEN_KEEPALIVE void set_mode(float value) {
    config.mode = value;
    if (!emu) {
        EM_ASM({
            setTheme($0);
        }, config.mode);
    }
}

EMSCRIPTEN_KEEPALIVE void edit_keybind(uint8_t value) {
    editing_keybind = value;
}

static void handle_input(void) {
    SDL_Event event;
    size_t len;
    char *savestate_path;
    char *rom_title;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_KEYDOWN:
            if (event.key.repeat)
                break;
            if (is_paused) {
                if (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_PAUSE) {
                    set_pause(SDL_FALSE);
                    if (editing_keybind >= JOYPAD_RIGHT && editing_keybind <= JOYPAD_START) {
                        EM_ASM({
                            toggleEditingKeybind($0);
                            editingKeybind = -1;
                        }, editing_keybind);
                        editing_keybind = -1;
                    }
                } else if (editing_keybind >= JOYPAD_RIGHT && editing_keybind <= JOYPAD_START && config.keycode_filter(event.key.keysym.sym)) {
                    // check if another keybind is already bound to this key and swap them if this is the case
                    for (int i = JOYPAD_RIGHT; i <= JOYPAD_START; i++) {
                        if (i != editing_keybind && (int) config.keybindings[i] == event.key.keysym.sym) {
                            config.keybindings[i] = config.keybindings[editing_keybind];
                            EM_ASM({
                                document.getElementById("keybind-setter-" + $0).innerHTML = UTF8ToString($1);
                            }, i, SDL_GetKeyName(config.keybindings[i]));
                            break;
                        }
                    }

                    config.keybindings[editing_keybind] = event.key.keysym.sym;
                    EM_ASM({
                        document.getElementById("keybind-setter-" + $0).innerHTML = UTF8ToString($1);
                        toggleEditingKeybind($0);
                        editingKeybind = -1;
                    }, editing_keybind, SDL_GetKeyName(config.keybindings[editing_keybind]));
                    editing_keybind = -1;
                }
                break;
            }
            switch (event.key.keysym.sym) {
            case SDLK_PAUSE:
            case SDLK_ESCAPE:
                set_pause(SDL_TRUE);
                break;
            case SDLK_F1: case SDLK_F2:
            case SDLK_F3: case SDLK_F4:
            case SDLK_F5: case SDLK_F6:
            case SDLK_F7: case SDLK_F8:
                if (!emu)
                    break;
                rom_title = emulator_get_rom_title(emu);
                len = strlen(rom_title);
                savestate_path = xmalloc(len + 10);
                snprintf(savestate_path, len + 9, "%s-state-%d", rom_title, event.key.keysym.sym - SDLK_F1);
                if (event.key.keysym.mod & KMOD_SHIFT) {
                    size_t savestate_length;
                    byte_t *savestate = emulator_get_savestate(emu, &savestate_length, 1);
                    local_storage_set_item(savestate_path, savestate, 1, savestate_length);
                    free(savestate);
                } else {
                    size_t savestate_length;
                    byte_t *savestate = local_storage_get_item(savestate_path, &savestate_length, 1);
                    int ret = emulator_load_savestate(emu, savestate, savestate_length);
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
            if (!is_paused)
                emulator_joypad_press(emu, keycode_to_joypad(&config, event.key.keysym.sym));
            break;
        case SDL_KEYUP:
            if (!event.key.repeat && !is_paused)
                emulator_joypad_release(emu, keycode_to_joypad(&config, event.key.keysym.sym));
            break;
        case SDL_CONTROLLERBUTTONDOWN:
            if (event.cbutton.button == SDL_CONTROLLER_BUTTON_GUIDE) {
                set_pause(!is_paused);
                if (editing_keybind >= 0) {
                    EM_ASM({
                        toggleEditingKeybind($0);
                        editingKeybind = -1;
                    }, editing_keybind);
                    editing_keybind = -1;
                }
                break;
            }
            if (!is_paused)
                emulator_joypad_press(emu, button_to_joypad(&config, event.cbutton.button));
            break;
        case SDL_CONTROLLERBUTTONUP:
            if (!is_paused)
                emulator_joypad_release(emu, button_to_joypad(&config, event.cbutton.button));
            break;
        case SDL_CONTROLLERDEVICEADDED:
            if (!is_controller_present) {
                pad = SDL_GameControllerOpen(event.cdevice.which);
                is_controller_present = SDL_TRUE;
                printf("%s connected\n", SDL_GameControllerName(pad));
            }
            break;
        case SDL_CONTROLLERDEVICEREMOVED:
            if (is_controller_present) {
                SDL_GameControllerClose(pad);
                is_controller_present = SDL_FALSE;
                printf("%s disconnected\n", SDL_GameControllerName(pad));
            }
            break;
        }
    }
}

static void loop(void);
static void paused_loop(void) {
    handle_input();
    if (!is_paused) {
        emscripten_cancel_main_loop();
        emscripten_set_main_loop(loop, 60, 1);
    }

    // update ppu_texture to show color palette changes behind the menu
    if (emu) {
        SDL_UpdateTexture(ppu_texture, NULL, emulator_get_pixels(emu), ppu_texture_pitch);
        SDL_RenderCopy(renderer, ppu_texture, NULL, NULL);
    }

    SDL_RenderPresent(renderer);
}

static void loop(void) {
    if (is_paused) {
        emscripten_cancel_main_loop();
        emscripten_set_main_loop(paused_loop, 30, 1);
    }

    SDL_RenderCopy(renderer, ppu_texture, NULL, NULL);
    SDL_RenderPresent(renderer);
    handle_input(); // keep this the closest possible before emulator_run_steps() to reduce input inaccuracies

    // run the emulator for the approximate number of steps it takes for the ppu to render a frame
    emulator_run_steps(emu, GB_CPU_STEPS_PER_FRAME * config.speed);
}

int main(int argc, char **argv) {
    // load_config() must be called before emulator_init()
    config.keybindings[JOYPAD_B] = SDLK_PERIOD; // change default B key to SDLK_PERIOD as SDLK_KP_PERIOD doesn't work for web
    load_config("config");
    emu = NULL;

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
        EM_ASM({
            document.getElementById("keybind-setter-" + $0).innerHTML = UTF8ToString($1);
        }, i, SDL_GetKeyName(config.keybindings[i]));
    }

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER);

    scale = config.scale;

    window = SDL_CreateWindow(
        EMULATOR_NAME,
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        GB_SCREEN_WIDTH * scale,
        GB_SCREEN_HEIGHT * scale,
        SDL_WINDOW_HIDDEN /*| SDL_WINDOW_RESIZABLE*/
    );
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_RenderClear(renderer);
    SDL_ShowWindow(window); // show window after creating the renderer to avoid weird window show -> hide -> show at startup

    ppu_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT);
    ppu_texture_pitch = GB_SCREEN_WIDTH * sizeof(byte_t) * 3;

    SDL_AudioSpec audio_settings = {
        .freq = GB_APU_SAMPLE_RATE,
        .format = AUDIO_F32SYS,
        .channels = GB_APU_CHANNELS,
        .samples = GB_APU_DEFAULT_SAMPLE_COUNT
    };
    audio_device = SDL_OpenAudioDevice(NULL, 0, &audio_settings, NULL, 0);
    SDL_PauseAudioDevice(audio_device, 0);

    emscripten_set_main_loop(paused_loop, 30, 1);

    return EXIT_SUCCESS;
}
