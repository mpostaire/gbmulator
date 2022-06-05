#include <stdlib.h>
#include <stdio.h>
#include <SDL2/SDL.h>

#include "ui.h"
#include "config.h"
#include "emulator/emulator.h"

// TODO fix pause menu when starting game link connexion while pause menu is still active (it's working but weirdly so low priority)

// TODO implemented MBCs have a few bugs (see https://github.com/drhelius/Gearboy to understand how its handled)

// FIXME dmg-acid2 test: everything good but missing exclamation mark. In fact it works for the 1st frame after
//       turning lcd on and it don't show for the other frames (as the ppu doesn't draw the first frame after lcd on,
//       it isn't visible). cgb-acid2 doesn't have this problem, the exclamation mark works well.

// TODO fix audio sync: it's "working" but I don't really know how and it's not perfect (good enough compromise of audio/video smoothness and sync)
// make audio sync to video (effectively replacing the audio sdl_delay by the vsync delay)
// TODO a cpu_step which do only 1 cycle at a time instead of instructions can improve audio syncing because a frame will always be the same ammount of cycles

// TODO pokemon red on gbc mode has wrong palettes

SDL_bool is_running = SDL_TRUE;
SDL_bool is_paused = SDL_TRUE;
SDL_bool is_rom_loaded = SDL_FALSE;

SDL_Window *window;

SDL_Texture *ppu_texture;
int ppu_texture_pitch;

SDL_AudioDeviceID audio_device;

SDL_GameController *pad;
SDL_bool is_controller_present = SDL_FALSE;

char window_title[sizeof(EMULATOR_NAME) + 19];

emulator_t *emu;

void gbmulator_exit(void) {
    is_running = SDL_FALSE;
}

void gbmulator_unpause(void) {
    if (is_rom_loaded)
        is_paused = SDL_FALSE;
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

static void ppu_vblank_cb(byte_t *pixels) {
    SDL_UpdateTexture(ppu_texture, NULL, pixels, ppu_texture_pitch);
}

static void apu_samples_ready_cb(float *audio_buffer, int audio_buffer_size) {
    while (SDL_GetQueuedAudioSize(audio_device) > audio_buffer_size * 8)
        SDL_Delay(1);
    SDL_QueueAudio(audio_device, audio_buffer, audio_buffer_size);
}

static char *get_xdg_path(const char *xdg_variable, const char *fallback) {
    char *xdg = getenv(xdg_variable);
    if (xdg) return xdg;

    char *home = getenv("HOME");
    size_t home_len = strlen(home);
    size_t fallback_len = strlen(fallback);
    char *buf = xmalloc(home_len + fallback_len + 3);
    snprintf(buf, home_len + fallback_len + 2, "%s/%s", home, fallback);
    return buf;
}

static char *get_config_path(void) {
    char *xdg_config = get_xdg_path("XDG_CONFIG_HOME", ".config");

    char *config_path = xmalloc(strlen(xdg_config) + 27);
    snprintf(config_path, strlen(xdg_config) + 26, "%s%s", xdg_config, "/gbmulator/gbmulator.conf");

    free(xdg_config);
    return config_path;
}

static char *get_save_path(const char *rom_filepath) {
    char *xdg_data = get_xdg_path("XDG_DATA_HOME", ".local/share");

    char *last_slash = strrchr(rom_filepath, '/');
    char *last_period = strrchr(last_slash ? last_slash : rom_filepath, '.');
    int last_period_index = last_period ? (int) (last_period - last_slash) : strlen(rom_filepath);

    size_t len = strlen(xdg_data) + strlen(last_slash ? last_slash : rom_filepath);
    char *save_path = xmalloc(len + 13);
    snprintf(save_path, len + 12, "%s/gbmulator%.*s.sav", xdg_data, last_period_index, last_slash);

    free(xdg_data);
    return save_path;
}

static char *get_savestate_path(const char *rom_filepath, int slot) {
    char *xdg_data = get_xdg_path("XDG_DATA_HOME", ".local/share");

    char *last_slash = strrchr(rom_filepath, '/');
    char *last_period = strrchr(last_slash ? last_slash : rom_filepath, '.');
    int last_period_index = last_period ? (int) (last_period - last_slash) : strlen(rom_filepath);

    size_t len = strlen(xdg_data) + strlen(last_slash);
    char *save_path = xmalloc(len + 33);
    snprintf(save_path, len + 32, "%s/gbmulator/savestates%.*s-%d.gbstate", xdg_data, last_period_index, last_slash, slot);

    free(xdg_data);
    return save_path;
}

static void handle_input(void) {
    SDL_Event event;
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
                if (!emu)
                    break;
                savestate_path = get_savestate_path(emulator_get_rom_path(emu), event.key.keysym.sym - SDLK_F1);
                if (event.key.keysym.mod & KMOD_SHIFT)
                    emulator_save_state(emu, savestate_path);
                else
                    emulator_load_state(emu, savestate_path);
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
        case SDL_QUIT:
            is_running = SDL_FALSE;
            break;
        }
    }
}

void gbmulator_load_cartridge(char *path) {
    char *rom_path;
    if (path)
        rom_path = path;
    else if (emu)
        rom_path = emulator_get_rom_path(emu);
    else
        return;

    if (emu) {
        char *save_path = get_save_path(rom_path);
        emulator_save(emu, save_path);
        free(save_path);
    }

    emulator_t *new_emu = emulator_init(config.mode, rom_path, ppu_vblank_cb, apu_samples_ready_cb);
    if (!new_emu) return;

    char *save_path = get_save_path(rom_path);
    emulator_load_save(new_emu, save_path);
    free(save_path);

    if (emu)
        emulator_quit(emu);
    emu = new_emu;

    emulator_set_apu_speed(emu, config.speed);
    emulator_set_apu_sound_level(emu, config.sound);
    emulator_set_color_palette(emu, config.color_palette);

    char *rom_title = emulator_get_rom_title(emu);
    snprintf(window_title, sizeof(window_title), EMULATOR_NAME" - %s", rom_title);
    SDL_SetWindowTitle(window, window_title);
    is_rom_loaded = SDL_TRUE;
    ui_enable_resume_button();
    ui_enable_link_button();
    ui_enable_reset_button();
    ui_back_to_main_menu();
    is_paused = SDL_FALSE;
}

int main(int argc, char **argv) {
    // must be called before emulator_init
    char *config_path = get_config_path();
    config_load(config_path);
    byte_t *ui_pixels = ui_init();
    emu = NULL;

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER);

    SDL_bool is_fullscreen = SDL_FALSE;
    byte_t scale = config.scale;

    window = SDL_CreateWindow(
        EMULATOR_NAME,
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        GB_SCREEN_WIDTH * scale,
        GB_SCREEN_HEIGHT * scale,
        SDL_WINDOW_HIDDEN /*| SDL_WINDOW_RESIZABLE*/
    );

    if (argc == 2)
        gbmulator_load_cartridge(argv[1]);

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_RenderSetLogicalSize(renderer, GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT);

    if (scale == 0) {
        is_fullscreen = SDL_TRUE;
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    }

    SDL_RenderClear(renderer);
    SDL_ShowWindow(window); // show window after creating the renderer to avoid weird window show -> hide -> show at startup

    ppu_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT);
    ppu_texture_pitch = GB_SCREEN_WIDTH * sizeof(byte_t) * 3;
    SDL_Texture *ui_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT);
    int ui_texture_pitch = GB_SCREEN_WIDTH * sizeof(byte_t) * 4;
    SDL_SetTextureBlendMode(ui_texture, SDL_BLENDMODE_BLEND);

    SDL_AudioSpec audio_settings = {
        .freq = GB_APU_SAMPLE_RATE,
        .format = AUDIO_F32SYS,
        .channels = GB_APU_CHANNELS,
        .samples = GB_APU_SAMPLE_COUNT
    };
    audio_device = SDL_OpenAudioDevice(NULL, 0, &audio_settings, NULL, 0);
    SDL_PauseAudioDevice(audio_device, 0);

    // main gbmulator loop
    int cycles = 0;
    while (is_running) {
        // emulation paused
        if (is_paused) {
            handle_input();
            if (!is_paused) // if user input disable pause, don't draw ui
                continue;

            // display menu
            ui_draw_menu();

            if (scale != config.scale) {
                scale = config.scale;
                if (scale == 0) {
                    is_fullscreen = SDL_TRUE;
                    SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
                } else {
                    if (is_fullscreen) {
                        is_fullscreen = SDL_FALSE;
                        SDL_SetWindowFullscreen(window, 0);
                    }
                    SDL_SetWindowSize(window, GB_SCREEN_WIDTH * scale, GB_SCREEN_HEIGHT * scale);
                }
            }

            if (is_fullscreen)
                SDL_RenderClear(renderer);

            // update ppu_texture to show color palette changes behind the menu
            if (is_rom_loaded) {
                SDL_UpdateTexture(ppu_texture, NULL, emulator_get_pixels(emu), ppu_texture_pitch);
                SDL_RenderCopy(renderer, ppu_texture, NULL, NULL);
            }

            SDL_UpdateTexture(ui_texture, NULL, ui_pixels, ui_texture_pitch);
            SDL_RenderCopy(renderer, ui_texture, NULL, NULL);

            SDL_RenderPresent(renderer);
            // delay to aim 30 fps (and avoid 100% CPU use) here because the normal delay is done by the sound emulation which is paused
            SDL_Delay(1.0f / 30.0f);
            continue;
        }

        // handle_input is a slow function: don't call it every step
        if (cycles >= GB_CPU_CYCLES_PER_FRAME * config.speed) {
            cycles = 0;
            if (is_fullscreen)
                SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, ppu_texture, NULL, NULL);
            SDL_RenderPresent(renderer);
            handle_input(); // keep this the closest possible before emulator_step() to reduce input inaccuracies
        }

        // run one step of the emulator
        cycles += emulator_step(emu);

        // no delay at the end of the loop because the emulation is audio synced (the audio is what makes the delay).
    }

    if (emu) {
        char *save_path = get_save_path(emulator_get_rom_path(emu));
        emulator_save(emu, save_path);
        free(save_path);
        emulator_quit(emu);
    }

    config_save(config_path);

    free(config_path);

    if (is_controller_present)
        SDL_GameControllerClose(pad);

    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyTexture(ppu_texture);
    SDL_DestroyTexture(ui_texture);

    SDL_CloseAudioDevice(audio_device);

    SDL_Quit();
    return EXIT_SUCCESS;
}
