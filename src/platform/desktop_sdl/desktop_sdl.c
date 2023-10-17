#include <stdlib.h>
#include <stdio.h>
#include <SDL2/SDL.h>

#include "ui.h"
#include "../common/link.h"
#include "../common/config.h"
#include "../common/utils.h"
#include "../../core/gb.h"
#include "../../core/gb_priv.h"

static int keycode_filter(SDL_Keycode key);
static void on_link_connect(gb_t *new_linked_gb);
static void on_link_disconnect(void);

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

static SDL_bool is_running = SDL_TRUE;
static SDL_bool is_paused = SDL_TRUE;

static SDL_Window *window;

static SDL_Texture *ppu_texture;
static int ppu_texture_pitch;

#define N_BUFFERS 8
#define N_SAMPLES 1024
#define SAMPLING_RATE 44100
static SDL_AudioDeviceID audio_device;
static gb_apu_sample_t samples[N_SAMPLES];
static int samples_count;

static SDL_GameController *pad;
static SDL_bool is_controller_present = SDL_FALSE;

static char window_title[sizeof(EMULATOR_NAME) + 19];

static ui_t *ui;
static char *rom_path;
static char *forced_save_path;

static int steps_per_frame = GB_CPU_STEPS_PER_FRAME;

static int sfd;

static gb_t *gb;
static gb_t *linked_gb;

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

static void gbmulator_exit(menu_entry_t *entry) {
    is_running = SDL_FALSE;
}

static void gbmulator_play(menu_entry_t *entry) {
    if (gb)
        is_paused = SDL_FALSE;
}

static void gbmulator_pause(menu_entry_t *entry) {
    is_paused = SDL_TRUE;
}

static void ppu_vblank_cb(const byte_t *pixels) {
    SDL_UpdateTexture(ppu_texture, NULL, pixels, ppu_texture_pitch);
}

#define DRC_TARGET_QUEUE_SIZE ((N_BUFFERS / 4) * N_SAMPLES)
#define DRC_MAX_FREQ_DIFF 0.02
#define DRC_ALPHA 0.1
static int ewma_queue_size;
static inline uint32_t dynamic_rate_control(int queue_size) {
    queue_size /= sizeof(gb_apu_sample_t);
    ewma_queue_size = queue_size * DRC_ALPHA + ewma_queue_size * (1.0 - DRC_ALPHA);

    double diff = (ewma_queue_size - DRC_TARGET_QUEUE_SIZE) / (double) DRC_TARGET_QUEUE_SIZE;
    return SAMPLING_RATE * (1.0 - CLAMP(diff, -1.0, 1.0) * DRC_MAX_FREQ_DIFF);
}
static void apu_new_sample_cb(const gb_apu_sample_t sample, uint32_t *dynamic_sampling_rate) {
    samples[samples_count++] = sample;
    if (samples_count < N_SAMPLES)
        return;
    samples_count = 0;

    Uint32 queue_size = SDL_GetQueuedAudioSize(audio_device);
    if (queue_size > N_BUFFERS * N_SAMPLES * sizeof(sample))
        return;
    SDL_QueueAudio(audio_device, &samples, sizeof(samples));

    *dynamic_sampling_rate = dynamic_rate_control(SDL_GetQueuedAudioSize(audio_device));
}

static void load_cartridge(char *path) {
    if (!gb && !path) return;

    if (gb) {
        char *save_path = get_save_path(rom_path);
        save_battery_to_file(gb, forced_save_path ? forced_save_path : save_path);
        free(save_path);
    }

    size_t rom_size;
    byte_t *rom;
    if (path) {
        size_t len = strlen(path);
        rom_path = xrealloc(rom_path, len + 2);
        snprintf(rom_path, len + 1, "%s", path);

        rom = get_rom(rom_path, &rom_size);
        if (!rom) {
            free(rom_path);
            rom_path = NULL;
            return;
        }
    } else {
        byte_t *data = gb_get_rom(gb, &rom_size);
        rom = xmalloc(rom_size);
        memcpy(rom, data, rom_size);
    }

    gb_options_t opts = {
        .mode = config.mode,
        .on_new_sample = apu_new_sample_cb,
        .on_new_frame = ppu_vblank_cb,
        .apu_speed = config.speed,
        .apu_sampling_rate = SAMPLING_RATE,
        .apu_sound_level = config.sound,
        .palette = config.color_palette
    };
    gb_t *new_emu = gb_init(rom, rom_size, &opts);
    free(rom);
    if (!new_emu) return;

    char *save_path = get_save_path(rom_path);
    load_battery_from_file(new_emu, forced_save_path ? forced_save_path : save_path);
    free(save_path);

    if (gb) {
        if (linked_gb) {
            gb_link_disconnect(gb);
            gb_quit(linked_gb);
            on_link_disconnect();
        }
        gb_quit(gb);
    }
    gb = new_emu;
    gb_print_status(gb);
    SDL_ClearQueuedAudio(audio_device);

    snprintf(window_title, sizeof(window_title), EMULATOR_NAME" - %s", gb_get_rom_title(gb));
    SDL_SetWindowTitle(window, window_title);

    ui->root_menu->entries[0].disabled = 0; // enable resume menu entry
    ui->root_menu->entries[2].disabled = 0; // enable reset rom menu entry
    ui->root_menu->entries[3].disabled = 0; // enable link cable menu entry

    gbmulator_play(NULL);
}









static void choose_scale(menu_entry_t *entry) {
    config.scale = entry->choices.position;
}

static void choose_speed(menu_entry_t *entry) {
    config.speed = (entry->choices.position * 0.5f) + 1;
    steps_per_frame = GB_CPU_STEPS_PER_FRAME * config.speed;
    if (gb)
        gb_set_apu_speed(gb, config.speed);
}

static void choose_sound(menu_entry_t *entry) {
    config.sound = entry->choices.position * 0.25f;
    if (gb)
        gb_set_apu_sound_level(gb, config.sound);
}

static void choose_color(menu_entry_t *entry) {
    config.color_palette = entry->choices.position;
    if (gb)
        gb_set_palette(gb, config.color_palette);
}

static void choose_mode(menu_entry_t *entry) {
    config.mode = entry->choices.position + 1;
}

static void choose_link_mode(menu_entry_t *entry) {
    entry->parent->entries[1].disabled = !entry->choices.position;
}

static void on_input_link_host(menu_entry_t *entry) {
    strncpy(config.link_host, entry->user_input.input, INET6_ADDRSTRLEN - 1);
    config.link_host[INET6_ADDRSTRLEN - 1] = '\0';
}

static void on_input_link_port(menu_entry_t *entry) {
    strncpy(config.link_port, entry->user_input.input, 5);
    config.link_port[5] = '\0';
}

static void start_link(menu_entry_t *entry) {
    if (!gb) return;

    if (entry->parent->entries[0].choices.position)
        sfd = link_connect_to_server(ui->config->link_host, ui->config->link_port);
    else
        sfd = link_start_server(ui->config->link_port);

    gb_t *new_linked_gb;
    if (sfd > 0 && link_init_transfer(sfd, gb, &new_linked_gb))
        on_link_connect(new_linked_gb);
}

static void open_rom(menu_entry_t *entry) {
    char filepath[1024] = { 0 };
    FILE *f = popen("zenity --file-selection", "r");
    if (!f) {
        errnoprintf("zenity");
        return;
    }
    fgets(filepath, 1024, f);
    pclose(f);
    filepath[strcspn(filepath, "\r\n")] = '\0';
    if (strlen(filepath))
        load_cartridge(filepath);
}

static void reset_rom(menu_entry_t *entry) {
    if (!gb)
        return;
    if (linked_gb) {
        gb_link_disconnect(gb);
        gb_quit(linked_gb);
        on_link_disconnect();
    }
    gb_reset(gb, config.mode);
    gb_print_status(gb);
    SDL_ClearQueuedAudio(audio_device);
    gbmulator_play(NULL);
}

menu_t link_menu = {
    .title = "Link cable",
    .length = 5,
    .entries = {
        { "Mode:     |server,client", UI_CHOICE, .choices = { choose_link_mode, 0 } },
        { "Host: ", UI_INPUT, .disabled = 1, .user_input.on_input = on_input_link_host },
        { "Port: ", UI_INPUT, .user_input = { .is_numeric = 1, .on_input = on_input_link_port } },
        { "Start link", UI_ACTION, .action = start_link },
        { "Back...", UI_BACK }
    }
};

menu_t options_menu = {
    .title = "Options",
    .length = 6,
    .entries = {
        { "Scale:      |Full, 1x , 2x , 3x , 4x , 5x ", UI_CHOICE, .choices = { choose_scale, 0 } },
        { "Speed:      |1.0x,1.5x,2.0x,2.5x,3.0x,3.5x,4.0x", UI_CHOICE, .choices = { choose_speed, 0 } },
        { "Sound:      | OFF, 25%, 50%, 75%,100%", UI_CHOICE, .choices = { choose_sound, 0 } },
        { "Color:      |gray,orig", UI_CHOICE, .choices = { choose_color, 0 } },
        { "Mode:       | DMG, CGB", UI_CHOICE, .choices = { choose_mode, 0 } },
        { "Back...", UI_BACK }
    }
};

menu_t keybindings_menu = {
    .title = "Keybindings",
    .length = 9,
    .entries = {
        { "LEFT:", UI_KEY_SETTER, .setter.button = JOYPAD_LEFT },
        { "RIGHT:", UI_KEY_SETTER, .setter.button = JOYPAD_RIGHT },
        { "UP:", UI_KEY_SETTER, .setter.button = JOYPAD_UP },
        { "DOWN:", UI_KEY_SETTER, .setter.button = JOYPAD_DOWN },
        { "A:", UI_KEY_SETTER, .setter.button = JOYPAD_A },
        { "B:", UI_KEY_SETTER, .setter.button = JOYPAD_B },
        { "START:", UI_KEY_SETTER, .setter.button = JOYPAD_START },
        { "SELECT:", UI_KEY_SETTER, .setter.button = JOYPAD_SELECT },
        { "Back...", UI_BACK }
    }
};

menu_t main_menu = {
    .title = "GBmulator",
    .length = 7,
    .entries = {
        { "Resume", UI_ACTION, .disabled = 1, .action = gbmulator_play },
        { "Open ROM...", UI_ACTION, .action = open_rom },
        { "Reset ROM", UI_ACTION, .disabled = 1, .action = reset_rom },
        { "Link cable...", UI_SUBMENU, .disabled = 1, .submenu = &link_menu },
        { "Options...", UI_SUBMENU, .submenu = &options_menu },
        { "Keybindings...", UI_SUBMENU, .submenu = &keybindings_menu },
        { "Exit", UI_ACTION, .action = gbmulator_exit }
    }
};





static void on_link_connect(gb_t *new_linked_gb) {
    linked_gb = new_linked_gb;
    gbmulator_play(NULL);
    config.speed = 1.0f;
    gb_set_apu_speed(gb, config.speed);

    link_menu.entries[0].disabled = 1;
    link_menu.entries[1].disabled = 1;
    link_menu.entries[2].disabled = 1;
    link_menu.entries[3].disabled = 1;

    options_menu.entries[1].disabled = 1;
}

static void on_link_disconnect(void) {
    linked_gb = NULL;

    link_menu.entries[0].disabled = 0;
    link_menu.entries[1].disabled = 0;
    link_menu.entries[2].disabled = 0;
    link_menu.entries[3].disabled = 0;

    options_menu.entries[1].disabled = 0;
}

static void handle_input(void) {
    SDL_Event event;
    char *savestate_path;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_TEXTINPUT:
            if (is_paused) {
                if (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_PAUSE) {
                    if (gb)
                        gbmulator_play(NULL);
                    else
                        ui_back_to_root_menu(ui);
                } else {
                    ui_text_input(ui, event.text.text);
                }
            }
            break;
        case SDL_KEYDOWN:
            if (is_paused) {
                if (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_PAUSE) {
                    if (gb)
                        gbmulator_play(NULL);
                    else
                        ui_back_to_root_menu(ui);
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
                gbmulator_pause(NULL);
                ui_back_to_root_menu(ui);
                break;
            case SDLK_F1: case SDLK_F2:
            case SDLK_F3: case SDLK_F4:
            case SDLK_F5: case SDLK_F6:
            case SDLK_F7: case SDLK_F8:
                if (!gb)
                    break;
                savestate_path = get_savestate_path(rom_path, event.key.keysym.sym - SDLK_F1);
                if (event.key.keysym.mod & KMOD_SHIFT) {
                    save_state_to_file(gb, savestate_path, 1);
                } else {
                    int ret = load_state_from_file(gb, savestate_path);
                    if (ret > 0) {
                        config.mode = ret;
                        options_menu.entries[4].choices.position = config.mode - 1;
                    }
                }
                free(savestate_path);
                break;
            }
            if (!is_paused)
                gb_joypad_press(gb, keycode_to_joypad(&config, event.key.keysym.sym));
            break;
        case SDL_KEYUP:
            if (!event.key.repeat && !is_paused)
                gb_joypad_release(gb, keycode_to_joypad(&config, event.key.keysym.sym));
            break;
        case SDL_CONTROLLERBUTTONDOWN:
            if (is_paused) {
                if (event.cbutton.button == SDL_CONTROLLER_BUTTON_GUIDE) {
                    if (gb)
                        gbmulator_play(NULL);
                    else
                        ui_back_to_root_menu(ui);
                } else {
                    ui_controller_press(ui, button_to_joypad(&config, event.cbutton.button));
                }
                break;
            }
            if (event.cbutton.button == SDL_CONTROLLER_BUTTON_GUIDE) {
                gbmulator_pause(NULL);
                ui_back_to_root_menu(ui);
                break;
            }
            if (!is_paused)
                gb_joypad_press(gb, button_to_joypad(&config, event.cbutton.button));
            break;
        case SDL_CONTROLLERBUTTONUP:
            if (!is_paused)
                gb_joypad_release(gb, button_to_joypad(&config, event.cbutton.button));
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

int main(int argc, char **argv) {
    char *config_path = get_config_path();
    // must be called before gb_init() and ui_init()
    config_load_from_file(&config, config_path);
    gb = NULL;
    steps_per_frame = GB_CPU_STEPS_PER_FRAME * config.speed;





    ui = ui_init(&main_menu, GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT, &config);
    options_menu.entries[0].choices.position = config.scale;
    options_menu.entries[1].choices.position = config.speed / 0.5f - 2;
    options_menu.entries[2].choices.position = config.sound * 4;
    options_menu.entries[3].choices.position = config.color_palette;
    options_menu.entries[4].choices.position = config.mode - 1;
    options_menu.entries[4].choices.description = xmalloc(16);
    snprintf(options_menu.entries[4].choices.description, 16, "Effect on reset");

    link_menu.entries[1].user_input.input = xmalloc(INET6_ADDRSTRLEN);
    snprintf(link_menu.entries[1].user_input.input, sizeof(config.link_host), "%s", config.link_host);

    link_menu.entries[1].user_input.cursor = strlen(config.link_host);
    link_menu.entries[1].user_input.max_length = 39;
    link_menu.entries[1].user_input.visible_hi = 12;

    char **link_port_buf = &link_menu.entries[2].user_input.input;
    *link_port_buf = xmalloc(6);

    link_menu.entries[2].user_input.cursor = snprintf(*link_port_buf, 6, "%s", config.link_port);
    link_menu.entries[2].user_input.input = *link_port_buf;
    link_menu.entries[2].user_input.max_length = 5;
    link_menu.entries[2].user_input.visible_hi = 5;










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

    if (argc > 1) {
        if (argc > 2)
            forced_save_path = argv[2];
        load_cartridge(argv[1]);
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    if (scale == 0) {
        is_fullscreen = SDL_TRUE;
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    }

    SDL_ShowWindow(window); // show window after creating the renderer to avoid weird window show -> hide -> show at startup

    ppu_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT);
    ppu_texture_pitch = GB_SCREEN_WIDTH * sizeof(byte_t) * 4;
    SDL_Texture *ui_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT);
    int ui_texture_pitch = GB_SCREEN_WIDTH * sizeof(byte_t) * 4;
    SDL_SetTextureBlendMode(ui_texture, SDL_BLENDMODE_BLEND);

    SDL_AudioSpec audio_settings = {
        .freq = SAMPLING_RATE,
        .format = AUDIO_S16SYS,
        .channels = 2,
        .samples = N_SAMPLES
    };
    audio_device = SDL_OpenAudioDevice(NULL, 0, &audio_settings, NULL, 0);
    SDL_PauseAudioDevice(audio_device, 0);

    // main gbmulator loop
    int steps = 0;
    while (is_running) {
        // emulation paused
        if (is_paused) {
            handle_input();
            if (!is_paused) // if user input disable pause, don't draw ui
                continue;

            // display ui
            ui_draw(ui);

            if (scale != config.scale) {
                scale = config.scale;
                if (scale == 0) {
                    is_fullscreen = SDL_TRUE;
                    SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
                    SDL_RenderSetLogicalSize(renderer, GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT);
                } else {
                    if (is_fullscreen) {
                        is_fullscreen = SDL_FALSE;
                        SDL_SetWindowFullscreen(window, 0);
                    }
                    SDL_SetWindowSize(window, GB_SCREEN_WIDTH * scale, GB_SCREEN_HEIGHT * scale);
                }
            }

            SDL_RenderClear(renderer);

            SDL_UpdateTexture(ui_texture, NULL, ui->pixels, ui_texture_pitch);
            SDL_RenderCopy(renderer, ui_texture, NULL, NULL);

            SDL_RenderPresent(renderer);
            // delay to match 30 fps to render the blinking elements of the ui but at a lower cpu usage
            SDL_Delay(1.0f / 30.0f);
            continue;
        }

        // handle_input is a slow function: don't call it every step
        if (steps >= steps_per_frame) {
            steps -= steps_per_frame;
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, ppu_texture, NULL, NULL);
            // this SDL_Delay() isn't needed as the audio sync adds it's own delay
            // TODO??? SDL_Delay((1.0f / 60.0f) - last_frame_time); // even with SDL waiting for vsync, delay here for monitors with different refresh rates than 60Hz
            SDL_RenderPresent(renderer);

            // keep this the closest possible before gb_step() to reduce input inaccuracies
            handle_input();
            if (linked_gb && !link_exchange_joypad(sfd, gb, linked_gb))
                on_link_disconnect();
        }

        // run one step of the emulator
        gb_step(gb);
        steps++;

        // no delay at the end of the loop because the emulation is audio synced (the audio is what makes the delay).
    }

    if (gb) {
        char *save_path = get_save_path(rom_path);
        save_battery_to_file(gb, forced_save_path ? forced_save_path : save_path);
        free(save_path);
        gb_quit(gb);
    }

    config_save_to_file(&config, config_path);
    free(config_path);

    ui_free(ui);

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
