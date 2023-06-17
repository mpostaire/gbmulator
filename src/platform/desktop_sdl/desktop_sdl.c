#include <stdlib.h>
#include <stdio.h>
#include <SDL2/SDL.h>

#include "ui.h"
#include "../common/link.h"
#include "../common/config.h"
#include "../common/utils.h"
#include "../../emulator/emulator.h"
#include "../../emulator/emulator_priv.h"

// TODO implemented MBCs have a few bugs (see https://github.com/drhelius/Gearboy to understand how its handled)

// FIXME dmg-acid2 test: everything good but missing exclamation mark. In fact it works for the 1st frame after
//       turning lcd on and it doesn't show for the other frames (as the ppu doesn't draw the first frame after lcd on,
//       it isn't visible). cgb-acid2 doesn't have this problem, the exclamation mark works well.

// TODO fix audio sync: it's "working" but I don't really know how and it's not perfect (good enough compromise of audio/video smoothness and sync)
// make audio sync to video (effectively replacing the audio sdl_delay by the vsync delay)

// TODO: in pokemon gold (in CGB and DMG modes) at the beginning animation of a battle, when the wild pokemon slides to the
// right, at the last moment, the top of the pokemon's sprite will appear for a few frames where the combat menu should be located

// TODO make tests and implement mooneye's test suite (and blargg)

static int keycode_filter(SDL_Keycode key);
static void on_link_connect(emulator_t *new_linked_emu);
static void on_link_disconnect(void);

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

SDL_bool is_running = SDL_TRUE;
SDL_bool is_paused = SDL_TRUE;

SDL_Window *window;

SDL_Texture *ppu_texture;
int ppu_texture_pitch;

SDL_AudioDeviceID audio_device;

SDL_GameController *pad;
SDL_bool is_controller_present = SDL_FALSE;

char window_title[sizeof(EMULATOR_NAME) + 19];

ui_t *ui;
char *rom_path;
char *forced_save_path;

int steps_per_frame = GB_CPU_STEPS_PER_FRAME;

int sfd;

emulator_t *emu;
emulator_t *linked_emu;

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

static void gbmulator_unpause(menu_entry_t *entry) {
    if (emu)
        is_paused = SDL_FALSE;
}

static void ppu_vblank_cb(const byte_t *pixels) {
    SDL_UpdateTexture(ppu_texture, NULL, pixels, ppu_texture_pitch);
}

static void apu_samples_ready_cb(const void *audio_buffer, int audio_buffer_size) {
    while (SDL_GetQueuedAudioSize(audio_device) > (unsigned int) (audio_buffer_size * 8))
        SDL_Delay(1);
    SDL_QueueAudio(audio_device, audio_buffer, audio_buffer_size);
}

static byte_t *get_rom_data(const char *path, size_t *rom_size) {
    const char *dot = strrchr(path, '.');
    if (!dot || (strncmp(dot, ".gb", MAX(strlen(dot), sizeof(".gb"))) && strncmp(dot, ".gbc", MAX(strlen(dot), sizeof(".gbc"))))) {
        eprintf("%s: wrong file extension (expected .gb or .gbc)\n", path);
        return NULL;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        errnoprintf("opening file %s", path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    size_t len = ftell(f);
    fseek(f, 0, SEEK_SET);

    byte_t *buf = xmalloc(len);
    if (!fread(buf, len, 1, f)) {
        errnoprintf("reading %s", path);
        fclose(f);
        return NULL;
    }
    fclose(f);

    if (rom_size)
        *rom_size = len;
    return buf;
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
    int last_period_index = last_period ? (int) (last_period - last_slash) : (int) strlen(rom_filepath);

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
    int last_period_index = last_period ? (int) (last_period - last_slash) : (int) strlen(rom_filepath);

    size_t len = strlen(xdg_data) + strlen(last_slash);
    char *save_path = xmalloc(len + 33);
    snprintf(save_path, len + 32, "%s/gbmulator/savestates%.*s-%d.gbstate", xdg_data, last_period_index, last_slash, slot);

    free(xdg_data);
    return save_path;
}

static void load_cartridge(char *path) {
    if (!emu && !path) return;

    if (emu) {
        char *save_path = get_save_path(rom_path);
        save_battery_to_file(emu, forced_save_path ? forced_save_path : save_path);
        free(save_path);
    }

    size_t rom_size;
    byte_t *rom_data;
    if (path) {
        size_t len = strlen(path);
        rom_path = xrealloc(rom_path, len + 2);
        snprintf(rom_path, len + 1, "%s", path);

        rom_data = get_rom_data(rom_path, &rom_size);
        if (!rom_data) {
            free(rom_path);
            rom_path = NULL;
            return;
        }
    } else {
        byte_t *data = emulator_get_rom(emu, &rom_size);
        rom_data = xmalloc(rom_size);
        memcpy(rom_data, data, rom_size);
    }

    emulator_options_t opts = {
        .mode = config.mode,
        .on_apu_samples_ready = apu_samples_ready_cb,
        .on_new_frame = ppu_vblank_cb,
        .apu_speed = config.speed,
        .apu_sound_level = config.sound,
        .palette = config.color_palette
    };
    emulator_t *new_emu = emulator_init(rom_data, rom_size, &opts);
    free(rom_data);
    if (!new_emu) return;

    char *save_path = get_save_path(rom_path);
    load_battery_from_file(new_emu, forced_save_path ? forced_save_path : save_path);
    free(save_path);

    if (emu) {
        if (linked_emu) {
            emulator_link_disconnect(emu);
            emulator_quit(linked_emu);
            on_link_disconnect();
        }
        emulator_quit(emu);
    }
    emu = new_emu;
    emulator_print_status(emu);

    snprintf(window_title, sizeof(window_title), EMULATOR_NAME" - %s", emulator_get_rom_title(emu));
    SDL_SetWindowTitle(window, window_title);

    ui->root_menu->entries[0].disabled = 0; // enable resume menu entry
    ui->root_menu->entries[2].disabled = 0; // enable reset rom menu entry
    ui->root_menu->entries[3].disabled = 0; // enable link cable menu entry

    is_paused = SDL_FALSE;
}









static void choose_scale(menu_entry_t *entry) {
    config.scale = entry->choices.position;
}

static void choose_speed(menu_entry_t *entry) {
    config.speed = (entry->choices.position * 0.5f) + 1;
    steps_per_frame = GB_CPU_STEPS_PER_FRAME * config.speed;
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
    if (emu)
        emulator_set_palette(emu, config.color_palette);
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
    if (!emu) return;

    if (entry->parent->entries[0].choices.position)
        sfd = link_connect_to_server(ui->config->link_host, ui->config->link_port);
    else
        sfd = link_start_server(ui->config->link_port);

    emulator_t *new_linked_emu;
    if (sfd > 0 && link_init_transfer(sfd, emu, &new_linked_emu))
        on_link_connect(new_linked_emu);
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
    if (!emu)
        return;
    if (linked_emu) {
        emulator_link_disconnect(emu);
        emulator_quit(linked_emu);
        on_link_disconnect();
    }
    emulator_reset(emu, config.mode);
    emulator_print_status(emu);
    is_paused = SDL_FALSE;
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
        { "Resume", UI_ACTION, .disabled = 1, .action = gbmulator_unpause },
        { "Open ROM...", UI_ACTION, .action = open_rom },
        { "Reset ROM", UI_ACTION, .disabled = 1, .action = reset_rom },
        { "Link cable...", UI_SUBMENU, .disabled = 1, .submenu = &link_menu },
        { "Options...", UI_SUBMENU, .submenu = &options_menu },
        { "Keybindings...", UI_SUBMENU, .submenu = &keybindings_menu },
        { "Exit", UI_ACTION, .action = gbmulator_exit }
    }
};





static void on_link_connect(emulator_t *new_linked_emu) {
    linked_emu = new_linked_emu;
    is_paused = SDL_FALSE;
    config.speed = 1.0f;

    link_menu.entries[0].disabled = 1;
    link_menu.entries[1].disabled = 1;
    link_menu.entries[2].disabled = 1;
    link_menu.entries[3].disabled = 1;

    options_menu.entries[1].disabled = 1;
}

static void on_link_disconnect(void) {
    linked_emu = NULL;

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
                    if (emu)
                        is_paused = SDL_FALSE;
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
                    if (emu)
                        is_paused = SDL_FALSE;
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
                is_paused = SDL_TRUE;
                ui_back_to_root_menu(ui);
                break;
            case SDLK_F1: case SDLK_F2:
            case SDLK_F3: case SDLK_F4:
            case SDLK_F5: case SDLK_F6:
            case SDLK_F7: case SDLK_F8:
                if (!emu)
                    break;
                savestate_path = get_savestate_path(rom_path, event.key.keysym.sym - SDLK_F1);
                if (event.key.keysym.mod & KMOD_SHIFT) {
                    save_state_to_file(emu, savestate_path, 1);
                } else {
                    int ret = load_state_from_file(emu, savestate_path);
                    if (ret > 0) {
                        config.mode = ret;
                        options_menu.entries[4].choices.position = config.mode - 1;
                    }
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
            if (is_paused) {
                if (event.cbutton.button == SDL_CONTROLLER_BUTTON_GUIDE) {
                    if (emu)
                        is_paused = SDL_FALSE;
                    else
                        ui_back_to_root_menu(ui);
                } else {
                    ui_controller_press(ui, button_to_joypad(&config, event.cbutton.button));
                }
                break;
            }
            if (event.cbutton.button == SDL_CONTROLLER_BUTTON_GUIDE) {
                is_paused = SDL_TRUE;
                ui_back_to_root_menu(ui);
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
        case SDL_QUIT:
            is_running = SDL_FALSE;
            break;
        }
    }
}

int main(int argc, char **argv) {
    char *config_path = get_config_path();
    // must be called before emulator_init() and ui_init()
    config_load_from_file(&config, config_path);
    emu = NULL;
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
        // TODO set render logical size for all fullscreen status changes and all window size changes
        // to check if it fixes the broken window after fullscreen bug
    }

    SDL_ShowWindow(window); // show window after creating the renderer to avoid weird window show -> hide -> show at startup

    ppu_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT);
    ppu_texture_pitch = GB_SCREEN_WIDTH * sizeof(byte_t) * 3;
    SDL_Texture *ui_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT);
    int ui_texture_pitch = GB_SCREEN_WIDTH * sizeof(byte_t) * 4;
    SDL_SetTextureBlendMode(ui_texture, SDL_BLENDMODE_BLEND);

    SDL_AudioSpec audio_settings = {
        .freq = GB_APU_SAMPLE_RATE,
        .format = AUDIO_F32SYS,
        .channels = GB_APU_CHANNELS,
        .samples = GB_APU_DEFAULT_SAMPLE_COUNT
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

            // update ppu_texture to show color palette changes behind the menu
            if (emu) {
                SDL_UpdateTexture(ppu_texture, NULL, emulator_get_pixels(emu), ppu_texture_pitch);
                SDL_RenderCopy(renderer, ppu_texture, NULL, NULL);
            }

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

            // keep this the closest possible before emulator_step() to reduce input inaccuracies
            handle_input();
            if (linked_emu && !link_exchange_joypad(sfd, emu, linked_emu))
                on_link_disconnect();
        }

        // run one step of the emulator
        emulator_step(emu);
        if (linked_emu)
            emulator_step(linked_emu);
        steps++;

        // no delay at the end of the loop because the emulation is audio synced (the audio is what makes the delay).
    }

    if (emu) {
        char *save_path = get_save_path(rom_path);
        save_battery_to_file(emu, forced_save_path ? forced_save_path : save_path);
        free(save_path);
        emulator_quit(emu);
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
