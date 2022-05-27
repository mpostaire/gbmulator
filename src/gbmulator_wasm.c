#include <stdlib.h>
#include <stdio.h>
#include <SDL2/SDL.h>
#include <emscripten.h>

#include "ui.h"
#include "config.h"
#include "emulator/emulator.h"
#include "base64.h"

SDL_bool is_paused = SDL_TRUE;
SDL_bool is_rom_loaded = SDL_FALSE;

SDL_Renderer *renderer;
SDL_Window *window;

SDL_Texture *ppu_texture;
int ppu_texture_pitch;

byte_t *ui_pixels_buffer;
SDL_Texture *ui_texture;
int ui_texture_pitch;

SDL_AudioDeviceID audio_device;

SDL_GameController *pad;
SDL_bool is_controller_present = SDL_FALSE;

char *rom_title;

byte_t scale;

char window_title[sizeof(EMULATOR_NAME) + 19];

emulator_t *emu;

void gbmulator_unpause(void) {
    if (is_rom_loaded)
        is_paused = SDL_FALSE;
}

static void ppu_vblank_cb(byte_t *pixels) {
    SDL_UpdateTexture(ppu_texture, NULL, pixels, ppu_texture_pitch);
}

static void apu_samples_ready_cb(float *audio_buffer, int audio_buffer_size) {
    SDL_QueueAudio(audio_device, audio_buffer, audio_buffer_size);
}

static byte_t *local_storage_get_item(const char *key, size_t *data_length) {
    unsigned char *data = (unsigned char *) EM_ASM_INT({
        var item = localStorage.getItem(UTF8ToString($0));
        if (item === null)
            return null;
        var itemLength = lengthBytesUTF8(item) + 1;

        var ret = _malloc(itemLength);
        stringToUTF8(item, ret, itemLength);
        return ret;
    }, key);
    if (!data) {
        *data_length = 0;
        return NULL;
    }
    unsigned char *decoded_data = base64_decode(data, strlen((char *) data), data_length);
    free(data);
    return decoded_data;
}

static void local_storage_set_item(const char *key, const byte_t *data) {
    EM_ASM({
        localStorage.setItem(UTF8ToString($0), UTF8ToString($1));
    }, key, data);
}

int sdl_key_to_joypad(SDL_Keycode key) {
    if (key == config.left) return JOYPAD_LEFT;
    if (key == config.right) return JOYPAD_RIGHT;
    if (key == config.up) return JOYPAD_UP;
    if (key == config.down) return JOYPAD_DOWN;
    if (key == config.a) return JOYPAD_A;
    if (key == config.b) return JOYPAD_B;
    if (key == config.start) return JOYPAD_START;
    if (key == config.select) return JOYPAD_SELECT;
    return key;
}

int sdl_controller_to_joypad(int button) {
    switch (button) {
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return JOYPAD_LEFT;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return JOYPAD_RIGHT;
    case SDL_CONTROLLER_BUTTON_DPAD_UP: return JOYPAD_UP;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return JOYPAD_DOWN;
    case SDL_CONTROLLER_BUTTON_A: return JOYPAD_A;
    case SDL_CONTROLLER_BUTTON_B: return JOYPAD_B;
    case SDL_CONTROLLER_BUTTON_START:
    case SDL_CONTROLLER_BUTTON_X:
        return JOYPAD_START;
    case SDL_CONTROLLER_BUTTON_BACK:
    case SDL_CONTROLLER_BUTTON_Y:
        return JOYPAD_SELECT;
    }
    return -1;
}

static void save(void) {
    size_t save_length;
    byte_t *save_data = emulator_get_save_data(emu, &save_length);
    if (!save_data) return;
    byte_t *save = base64_encode(save_data, save_length, NULL);
    local_storage_set_item(rom_title, save);
    free(save_data);
    free(save);
}

EMSCRIPTEN_KEEPALIVE void on_before_unload(void) {
    if (emu)
        emulator_quit(emu);
    if (rom_title)
        free(rom_title);

    config_save("config");

    if (is_controller_present)
        SDL_GameControllerClose(pad);

    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyTexture(ppu_texture);
    SDL_DestroyTexture(ui_texture);

    SDL_Quit();
}

EMSCRIPTEN_KEEPALIVE void on_gui_button_down(joypad_button_t button) {
    if (is_paused)
        ui_press_joypad(button);
    else
        emulator_joypad_press(emu, button);
}

EMSCRIPTEN_KEEPALIVE void on_gui_button_up(joypad_button_t button) {
    if (!is_paused)
        emulator_joypad_release(emu, button);
}

EMSCRIPTEN_KEEPALIVE void receive_rom_data(uint8_t *rom_data, size_t rom_size) {
    if (emu) {
        save();
        free(rom_title);
        emulator_quit(emu);
    }

    rom_title = emulator_get_rom_title_from_data(rom_data, rom_size);
    for (int i = 0; i < 16; i++)
        if (rom_title[i] == ' ')
            rom_title[i] = '_';

    emu = emulator_init_from_data(rom_data, rom_size, NULL, ppu_vblank_cb, apu_samples_ready_cb);
    if (!emu) return;

    size_t save_length;
    unsigned char *save = local_storage_get_item(rom_title, &save_length);
    if (save) {
        emulator_load_save_data(emu, save, save_length);
        free(save);
    }

    emulator_set_apu_speed(emu, config.speed);
    emulator_set_apu_sound_level(emu, config.sound);
    emulator_set_color_palette(emu, config.color_palette);

    snprintf(window_title, sizeof(window_title), EMULATOR_NAME" - %s", rom_title);
    SDL_SetWindowTitle(window, window_title);
    is_rom_loaded = SDL_TRUE;
    ui_enable_resume_button();
    ui_back_to_main_menu();
    is_paused = SDL_FALSE;

    free(rom_data);
    rom_size = 0;
}

static void handle_input(void) {
    SDL_Event event;
    size_t len;
    char *savestate_path;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_TEXTINPUT:
            if (is_paused)
                ui_text_input(event.text.text);
            break;
        case SDL_KEYDOWN:
            if (is_paused) {
                ui_keyboard_press(&event.key);
                break;
            }
            if (event.key.repeat)
                break;
            switch (event.key.keysym.sym) {
            case SDLK_PAUSE:
            case SDLK_ESCAPE:
                is_paused = SDL_TRUE;
                break;
            case SDLK_F1: case SDLK_F2:
            case SDLK_F3: case SDLK_F4:
            case SDLK_F5: case SDLK_F6:
            case SDLK_F7: case SDLK_F8:
                if (!rom_title)
                    break;
                len = strlen(rom_title);
                savestate_path = xmalloc(len + 10);
                snprintf(savestate_path, len + 9, "%s-state-%d", rom_title, event.key.keysym.sym - SDLK_F1);
                if (event.key.keysym.mod & KMOD_SHIFT) {
                    size_t savestate_length;
                    byte_t *savestate = emulator_get_state_data(emu, &savestate_length);
                    byte_t *encoded_savestate = base64_encode(savestate, savestate_length, NULL);
                    local_storage_set_item(savestate_path, encoded_savestate);
                    free(encoded_savestate);
                    free(savestate);
                } else {
                    size_t savestate_length;
                    byte_t *savestate = local_storage_get_item(savestate_path, &savestate_length);
                    emulator_load_state_data(emu, savestate, savestate_length);
                    free(savestate);
                }
                free(savestate_path);
                break;
            }
            if (!is_paused)
                emulator_joypad_press(emu, sdl_key_to_joypad(event.key.keysym.sym));
            break;
        case SDL_KEYUP:
            if (!event.key.repeat && !is_paused)
                emulator_joypad_release(emu, sdl_key_to_joypad(event.key.keysym.sym));
            break;
        case SDL_CONTROLLERBUTTONDOWN:
            if (is_paused) {
                ui_controller_press(event.cbutton.button);
                break;
            }
            if (event.cbutton.button == SDL_CONTROLLER_BUTTON_GUIDE) {
                is_paused = SDL_TRUE;
                break;
            }
            if (!is_paused)
                emulator_joypad_press(emu, sdl_controller_to_joypad(event.cbutton.button));
            break;
        case SDL_CONTROLLERBUTTONUP:
            if (!is_paused)
                emulator_joypad_release(emu, sdl_controller_to_joypad(event.cbutton.button));
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
    if (!is_paused) {
        emscripten_cancel_main_loop();
        emscripten_set_main_loop(loop, 60, 1);
    }

    handle_input();
    if (!is_paused) { // if user input disable pause, don't draw ui
        emscripten_cancel_main_loop();
        emscripten_set_main_loop(loop, 60, 1);
    }

    // display menu
    ui_draw_menu();

    if (scale != config.scale) {
        scale = config.scale;
        SDL_SetWindowSize(window, GB_SCREEN_WIDTH * scale, GB_SCREEN_HEIGHT * scale);
    }

    // update ppu_texture to show color palette changes behind the menu
    if (is_rom_loaded) {
        SDL_UpdateTexture(ppu_texture, NULL, emulator_get_pixels(emu), ppu_texture_pitch);
        SDL_RenderCopy(renderer, ppu_texture, NULL, NULL);
    }

    SDL_UpdateTexture(ui_texture, NULL, ui_pixels_buffer, ui_texture_pitch);
    SDL_RenderCopy(renderer, ui_texture, NULL, NULL);

    SDL_RenderPresent(renderer);
}

static void loop(void) {
    if (is_paused) {
        emscripten_cancel_main_loop();
        emscripten_set_main_loop(paused_loop, 30, 1);
    }

    SDL_RenderCopy(renderer, ppu_texture, NULL, NULL);
    SDL_RenderPresent(renderer);
    handle_input(); // keep this the closest possible before emulator_step() to reduce input inaccuracies

    // run the emulator for the approximate number of cycles it takes for the ppu to render a frame
    emulator_run_cycles(emu, GB_CPU_CYCLES_PER_FRAME * config.speed);
}

int main(int argc, char **argv) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER);

    config_load("config");
    emu = NULL;
    rom_title = NULL;

    ui_pixels_buffer = ui_init();

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
    ui_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT);
    ui_texture_pitch = GB_SCREEN_WIDTH * sizeof(byte_t) * 4;
    SDL_SetTextureBlendMode(ui_texture, SDL_BLENDMODE_BLEND);

    SDL_AudioSpec audio_settings = {
        .freq = GB_APU_SAMPLE_RATE,
        .format = AUDIO_F32SYS,
        .channels = 2,
        .samples = GB_APU_SAMPLE_COUNT
    };
    audio_device = SDL_OpenAudioDevice(NULL, 0, &audio_settings, NULL, 0);
    SDL_PauseAudioDevice(audio_device, 0);

    emscripten_set_main_loop(paused_loop, 30, 1);

    return EXIT_SUCCESS;
}
