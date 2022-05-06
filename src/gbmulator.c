#include <stdlib.h>
#include <stdio.h>
#include <SDL2/SDL.h>

#include "ui.h"
#include "config.h"
#include "emulator/emulator.h"

// FIXME LY==LYC interrupt buggy, in fact most of the ppu is (check argentum emulator ppu code to understand how its handled)

// TODO when no argument, show alternative menu with title Insert cartridge, ACTION: zenity open ROM from files, label: Or drag and drop ROM

// TODO fix pause menu when starting game link connexion while pause menu is still active (it's working but weirdly so low priority)

// TODO MBCs are poorly implemented (see https://github.com/drhelius/Gearboy to understand its handled)

// TODO compartimentalize everyting (and replace all the extern variables by something not accessible) to make loading new roms when one is already playing possible

SDL_bool is_running = SDL_TRUE;
SDL_bool is_paused = SDL_FALSE;

SDL_Renderer *renderer;

SDL_Texture *ppu_texture;
int ppu_texture_pitch;
SDL_AudioDeviceID audio_device;

void gbmulator_exit(void) {
    is_running = SDL_FALSE;
}

void gbmulator_unpause(void) {
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

static void handle_input(void) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_TEXTINPUT:
            if (is_paused)
                ui_text_input(event.text.text);
            break;
        case SDL_KEYDOWN:
            if (is_paused)
                ui_press(&event.key.keysym);
            if (event.key.repeat)
                break;
            switch (event.key.keysym.sym) {
            case SDLK_PAUSE:
            case SDLK_ESCAPE:
                if (is_paused)
                    ui_back_to_main_menu();
                is_paused = !is_paused;
                break;
            case SDLK_F1: case SDLK_F2:
            case SDLK_F3: case SDLK_F4:
            case SDLK_F5: case SDLK_F6:
            case SDLK_F7: case SDLK_F8:
                char buf[256];
                snprintf(buf, sizeof(buf), "%s-%d.gbstate", emulator_get_rom_path(), event.key.keysym.sym - SDLK_F1);
                if (event.key.keysym.mod & KMOD_SHIFT)
                    emulator_load_state(buf);
                else
                    emulator_save_state(buf);
                break;
            }
            joypad_press(sdl_key_to_joypad(event.key.keysym.sym));
            break;
        case SDL_KEYUP:
            if (!event.key.repeat)
                joypad_release(sdl_key_to_joypad(event.key.keysym.sym));
            break;
        case SDL_QUIT:
            is_running = SDL_FALSE;
            break;
        }
    }
}

static char *get_save_filepath(const char *rom_filepath) {
    size_t len = strlen(rom_filepath);
    char *buf = xmalloc(len + 2);

    char *last_period = strrchr(rom_filepath, '.');
    int last_period_index = (int) (last_period - rom_filepath);
    snprintf(buf, len + 2, "%.*s.sav", last_period_index, rom_filepath);

    return buf;
}

static void ppu_vblank_cb(byte_t *pixels) {
    SDL_UpdateTexture(ppu_texture, NULL, pixels, ppu_texture_pitch);
}

static void apu_samples_ready_cb(float *audio_buffer, int audio_buffer_size) {
    while (SDL_GetQueuedAudioSize(audio_device) > audio_buffer_size)
        SDL_Delay(1);
    SDL_QueueAudio(audio_device, audio_buffer, audio_buffer_size);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: %s path/to/rom.gb\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *save_path = get_save_filepath(argv[1]);
    emulator_init(argv[1], save_path, ppu_vblank_cb, apu_samples_ready_cb);
    emulator_set_apu_sound_level(config.sound);
    emulator_set_apu_sampling_freq_multiplier(config.speed);
    char *rom_title = emulator_get_rom_title();
    char window_title[sizeof(EMULATOR_NAME) + 19];
    snprintf(window_title, sizeof(window_title), EMULATOR_NAME" - %s", rom_title);
    printf("Playing %s\n", rom_title);

    // must be called after emulator_init because otherwise emualtor_init would overwrite some of the config
    // like palette, sound, etc.
    const char *config_path = config_load();

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

    byte_t *ui_pixels = ui_init();

    byte_t scale = config.scale;

    SDL_Window *window = SDL_CreateWindow(
        window_title,
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        GB_SCREEN_WIDTH * scale,
        GB_SCREEN_HEIGHT * scale,
        SDL_WINDOW_HIDDEN /*| SDL_WINDOW_RESIZABLE*/
    );
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_ShowWindow(window); // show window after creating the renderer to avoid weird window show -> hide -> show at startup

    ppu_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT);
    ppu_texture_pitch = GB_SCREEN_WIDTH * sizeof(byte_t) * 3;
    SDL_Texture *ui_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT);
    int ui_texture_pitch = GB_SCREEN_WIDTH * sizeof(byte_t) * 4;
    SDL_SetTextureBlendMode(ui_texture, SDL_BLENDMODE_BLEND);

    SDL_AudioSpec audio_settings = {
        .freq = APU_SAMPLE_RATE,
        .format = AUDIO_F32SYS,
        .channels = 2,
        .samples = APU_SAMPLE_COUNT
    };
    audio_device = SDL_OpenAudioDevice(NULL, 0, &audio_settings, NULL, 0);
    SDL_PauseAudioDevice(audio_device, 0);

    // main gbmulator loop
    int cycles = 0;
    while (is_running) {
        // emulation paused
        if (is_paused) {
            handle_input();

            // display menu
            ui_draw_menu();

            if (scale != config.scale) {
                scale = config.scale;
                SDL_SetWindowSize(window, GB_SCREEN_WIDTH * scale, GB_SCREEN_HEIGHT * scale);
            }

            // update ppu_texture to show color palette changes behind the menu
            SDL_UpdateTexture(ppu_texture, NULL, emulator_ppu_get_pixels(), ppu_texture_pitch);
            SDL_RenderCopy(renderer, ppu_texture, NULL, NULL);

            SDL_UpdateTexture(ui_texture, NULL, ui_pixels, ui_texture_pitch);
            SDL_RenderCopy(renderer, ui_texture, NULL, NULL);

            SDL_RenderPresent(renderer);
            // delay to aim 30 fps (and avoid 100% CPU use) here because the normal delay is done by the sound emulation which is paused
            SDL_Delay(1 / 30.0);
            continue;
        }

        // handle_input is a slow function: don't call it every step
        if (cycles >= GB_CPU_CYCLES_PER_FRAME * config.speed) {
            cycles = 0;
            SDL_RenderCopy(renderer, ppu_texture, NULL, NULL);
            SDL_RenderPresent(renderer);
            handle_input(); // keep this the closest possible before emulator_step() to reduce input inaccuracies
        }

        // run on step of the emulator
        cycles += emulator_step();

        // no delay at the end of the loop because the emulation is audio synced (the audio is what makes the delay).
    }

    emulator_quit();

    config_save(config_path);

    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyTexture(ppu_texture);
    SDL_DestroyTexture(ui_texture);

    SDL_Quit();
    return EXIT_SUCCESS;
}
