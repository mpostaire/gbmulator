#include <stdlib.h>
#include <stdio.h>
#include <SDL2/SDL.h>
#include <emscripten.h>

#include "../common/ui.h"
#include "../common/config.h"
#include "../common/utils.h"
#include "emulator/emulator.h"
#include "base64.h"

SDL_bool is_paused = SDL_TRUE;
SDL_bool is_rom_loaded = SDL_FALSE;

SDL_Renderer *renderer;
SDL_Window *window;

SDL_Texture *ppu_texture;
int ppu_texture_pitch;

SDL_Texture *ui_texture;
int ui_texture_pitch;

SDL_AudioDeviceID audio_device;

SDL_GameController *pad;
SDL_bool is_controller_present = SDL_FALSE;

char *rom_title;

byte_t scale;

char window_title[sizeof(EMULATOR_NAME) + 19];

ui_t *ui;
emulator_t *emu;

void gbmulator_unpause(menu_entry_t *entry) {
    if (is_rom_loaded)
        is_paused = SDL_FALSE;
}

static void ppu_vblank_cb(byte_t *pixels) {
    SDL_UpdateTexture(ppu_texture, NULL, pixels, ppu_texture_pitch);
}

static void apu_samples_ready_cb(float *audio_buffer, int audio_buffer_size) {
    while (SDL_GetQueuedAudioSize(audio_device) > audio_buffer_size * 8)
        emscripten_sleep(1);
    SDL_QueueAudio(audio_device, audio_buffer, audio_buffer_size);
}

static byte_t *local_storage_get_item(const char *key, size_t *data_length, byte_t decode) {
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
            localStorage.setItem(UTF8ToString($0), UTF8ToString($1));
        }, key, encoded_data);
        free(encoded_data);
    } else {
        EM_ASM({
            localStorage.setItem(UTF8ToString($0), UTF8ToString($1));
        }, key, data);
    }
}

void load_config(char *path) {
    byte_t *data = local_storage_get_item(path, NULL, 0);
    if (!data)
        return;
    config_load_from_buffer((const char *) data);
    free(data);
}

static void save_config(const char *path) {
    size_t len;
    char *config_buf = config_save_to_buffer(&len);
    local_storage_set_item(path, (byte_t *) config_buf, 0, len);
    free(config_buf);
}

static void save(void) {
    size_t save_length;
    byte_t *save_data = emulator_get_save_data(emu, &save_length);
    if (!save_data) return;
    local_storage_set_item(rom_title, save_data, 1, save_length);
    free(save_data);
}

void load_cartridge(const byte_t *rom_data, size_t rom_size, char *new_rom_title) {
    emulator_t *new_emu = emulator_init_from_data(config.mode, rom_data, rom_size, ppu_vblank_cb, apu_samples_ready_cb);
    if (!new_emu) return;

    if (emu) {
        save();
        free(rom_title);
        emulator_quit(emu);
    }
    rom_title = new_rom_title;
    emu = new_emu;

    size_t save_length;
    unsigned char *save = local_storage_get_item(rom_title, &save_length, 1);
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
    is_paused = SDL_FALSE;

    ui->root_menu->entries[0].disabled = 0; // enable resume menu entry
    ui->root_menu->entries[2].disabled = 0; // enable reset rom menu entry

    EM_ASM({
        setTheme($0);
    }, config.mode);
}

EMSCRIPTEN_KEEPALIVE void on_before_unload(void) {
    if (emu) {
        save();
        emulator_quit(emu);
    }
    if (rom_title)
        free(rom_title);

    // save config
    save_config("config");

    if (is_controller_present)
        SDL_GameControllerClose(pad);

    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyTexture(ppu_texture);
    SDL_DestroyTexture(ui_texture);

    SDL_CloseAudioDevice(audio_device);

    SDL_Quit();
}

EMSCRIPTEN_KEEPALIVE void on_gui_button_down(joypad_button_t button) {
    if (is_paused)
        ui_press_joypad(ui, button);
    else
        emulator_joypad_press(emu, button);
}

EMSCRIPTEN_KEEPALIVE void on_gui_button_up(joypad_button_t button) {
    if (!is_paused)
        emulator_joypad_release(emu, button);
}

// TODO receive_rom_data() and gbmulator_reset() are very similar: a lot of code can be made into another function
EMSCRIPTEN_KEEPALIVE void receive_rom_data(uint8_t *rom_data, size_t rom_size) {
    char *new_rom_title = emulator_get_rom_title_from_data(rom_data, rom_size);
    for (int i = 0; i < 16; i++)
        if (new_rom_title[i] == ' ')
            new_rom_title[i] = '_';

    load_cartridge(rom_data, rom_size, new_rom_title);
    free(rom_data);
}

void gbmulator_reset(void) {
    if (!emu)
        return;

    size_t rom_size;
    byte_t *cart = emulator_get_rom_data(emu, &rom_size);
    byte_t *rom_data = xmalloc(rom_size);
    memcpy(rom_data, cart, rom_size);

    char *new_rom_title = emulator_get_rom_title_from_data(rom_data, rom_size);
    for (int i = 0; i < 16; i++)
        if (new_rom_title[i] == ' ')
            new_rom_title[i] = '_';

    load_cartridge(rom_data, rom_size, new_rom_title);
    free(rom_data);
}

static void handle_input(void) {
    SDL_Event event;
    size_t len;
    char *savestate_path;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_TEXTINPUT:
            if (is_paused) {
                if (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_PAUSE) {
                    if (is_rom_loaded) {
                        is_paused = SDL_FALSE;
                    } else {
                        ui->current_menu = ui->root_menu;
                        ui_set_position(ui, 0, 0);
                    }
                } else {
                    ui_text_input(ui, event.text.text);
                }
            }
            break;
        case SDL_KEYDOWN:
            if (is_paused) {
                if (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_PAUSE) {
                    if (is_rom_loaded) {
                        is_paused = SDL_FALSE;
                    } else {
                        ui->current_menu = ui->root_menu;
                        ui_set_position(ui, 0, 0);
                    }
                } else {
                    ui_keyboard_press(ui, &event.key);
                }
                break;
            }
            if (event.key.repeat)
                break;
            switch (event.key.keysym.sym) {
            case SDLK_PAUSE:
            case SDLK_ESCAPE:
                is_paused = SDL_TRUE;
                ui->current_menu = ui->root_menu;
                ui_set_position(ui, 0, 0);
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
                    local_storage_set_item(savestate_path, savestate, 1, savestate_length);
                    free(savestate);
                } else {
                    size_t savestate_length;
                    byte_t *savestate = local_storage_get_item(savestate_path, &savestate_length, 1);
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
                if (event.cbutton.button == SDL_CONTROLLER_BUTTON_GUIDE) {
                    if (is_rom_loaded) {
                        is_paused = SDL_FALSE;
                    } else {
                        ui->current_menu = ui->root_menu;
                        ui_set_position(ui, 0, 0);
                    }
                } else {
                    ui_controller_press(ui, event.cbutton.button);
                }
                break;
            }
            if (event.cbutton.button == SDL_CONTROLLER_BUTTON_GUIDE) {
                is_paused = SDL_TRUE;
                ui->current_menu = ui->root_menu;
                ui_set_position(ui, 0, 0);
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
    ui_draw(ui);

    if (scale != config.scale) {
        scale = config.scale;
        SDL_SetWindowSize(window, GB_SCREEN_WIDTH * scale, GB_SCREEN_HEIGHT * scale);
    }

    // update ppu_texture to show color palette changes behind the menu
    if (is_rom_loaded) {
        SDL_UpdateTexture(ppu_texture, NULL, emulator_get_pixels(emu), ppu_texture_pitch);
        SDL_RenderCopy(renderer, ppu_texture, NULL, NULL);
    }

    SDL_UpdateTexture(ui_texture, NULL, ui->pixels, ui_texture_pitch);
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








static void choose_scale(menu_entry_t *entry) {
    config.scale = entry->choices.position + 2;
}

static void choose_speed(menu_entry_t *entry) {
    config.speed = (entry->choices.position * 0.5f) + 1;
    if (emu)
        emulator_set_apu_speed(emu, config.speed);
}

static void choose_sound(menu_entry_t *entry) {
    config.sound = entry->choices.position * 0.25f;
    if (emu)
        emulator_set_apu_sound_level(emu, config.sound);
}

static void choose_color(menu_entry_t *entry) {
    config.color_palette = entry->choices.position;
    if (emu) {
        emulator_update_pixels_with_palette(emu, config.color_palette);
        emulator_set_color_palette(emu, config.color_palette);
    }
}

static void choose_mode(menu_entry_t *entry) {
    config.mode = entry->choices.position;
    if (!emu) {
        EM_ASM({
            setTheme($0);
        }, config.mode);
    }
}

static void open_rom(menu_entry_t *entry) {
    EM_ASM(
        var file_selector = document.createElement('input');
        file_selector.setAttribute('type', 'file');
        file_selector.setAttribute('onchange','openFile(event)');
        file_selector.setAttribute('accept','.gb,.gbc'); // optional - limit accepted file types
        file_selector.click();
    );
}

static void reset_rom(menu_entry_t *entry) {
    gbmulator_reset();
}


menu_t options_menu = {
    .title = "Options",
    .length = 6,
    .entries = {
        { "Scale:      | 2x , 3x , 4x ", UI_CHOICE, .choices = { choose_scale, 3, 0 } },
        { "Speed:      |1.0x,1.5x,2.0x,2.5x,3.0x,3.5x,4.0x", UI_CHOICE, .choices = { choose_speed, 7, 0 } },
        { "Sound:      | OFF, 25%, 50%, 75%,100%", UI_CHOICE, .choices = { choose_sound, 5, 0 } },
        { "Color:      |gray,orig", UI_CHOICE, .choices = { choose_color, 2, 0 } },
        { "Mode:       | DMG, CGB", UI_CHOICE, .choices = { choose_mode, 2, 0 } },
        { "Back...", UI_BACK }
    }
};

menu_t keybindings_menu = {
    .title = "Keybindings",
    .length = 9,
    .entries = {
        { "LEFT:", UI_KEY_SETTER, .setter.config_key = &config.left },
        { "RIGHT:", UI_KEY_SETTER, .setter.config_key = &config.right },
        { "UP:", UI_KEY_SETTER, .setter.config_key = &config.up },
        { "DOWN:", UI_KEY_SETTER, .setter.config_key = &config.down },
        { "A:", UI_KEY_SETTER, .setter.config_key = &config.a },
        { "B:", UI_KEY_SETTER, .setter.config_key = &config.b },
        { "START:", UI_KEY_SETTER, .setter.config_key = &config.start },
        { "SELECT:", UI_KEY_SETTER, .setter.config_key = &config.select },
        { "Back...", UI_BACK }
    }
};

menu_t main_menu = {
    .title = "GBmulator",
    .length = 5,
    .entries = {
        { "Resume", UI_ACTION, .disabled = 1, .action = gbmulator_unpause },
        { "Open ROM...", UI_ACTION, .action = open_rom },
        { "Reset ROM", UI_ACTION, .disabled = 1, .action = reset_rom },
        { "Options...", UI_SUBMENU, .submenu = &options_menu },
        { "Keybindings...", UI_SUBMENU, .submenu = &keybindings_menu },
    }
};












int main(int argc, char **argv) {
    // load_config() must be called before emulator_init() and ui_init()
    config.b = SDLK_PERIOD; // change default B key to SDLK_PERIOD as SDLK_KP_PERIOD doesn't work for web
    load_config("config");
    emu = NULL;
    rom_title = NULL;





    ui = ui_init(&main_menu, GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT);
    options_menu.entries[0].choices.position = config.scale - 2;
    options_menu.entries[1].choices.position = config.speed / 0.5f - 2;
    options_menu.entries[2].choices.position = config.sound * 4;
    options_menu.entries[3].choices.position = config.color_palette;
    options_menu.entries[4].choices.position = config.mode;
    options_menu.entries[4].choices.description = xmalloc(16);
    snprintf(options_menu.entries[4].choices.description, 16, "Effect on reset");





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
    ui_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT);
    ui_texture_pitch = GB_SCREEN_WIDTH * sizeof(byte_t) * 4;
    SDL_SetTextureBlendMode(ui_texture, SDL_BLENDMODE_BLEND);

    SDL_AudioSpec audio_settings = {
        .freq = GB_APU_SAMPLE_RATE,
        .format = AUDIO_F32SYS,
        .channels = GB_APU_CHANNELS,
        .samples = GB_APU_SAMPLE_COUNT
    };
    audio_device = SDL_OpenAudioDevice(NULL, 0, &audio_settings, NULL, 0);
    SDL_PauseAudioDevice(audio_device, 0);

    emscripten_set_main_loop(paused_loop, 30, 1);

    return EXIT_SUCCESS;
}
