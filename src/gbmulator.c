#include <stdlib.h>
#include <stdio.h>
#include <SDL2/SDL.h>

#include "ui.h"
#include "config.h"
#include "emulator/emulator.h"

#define WINDOW_TITLE "GBmulator"

// FIXME LY==LYC interrupt buggy, in fact most of the ppu is (check argentum emulator ppu code to see how its handled)

// TODO when no argument, show alternative menu with title Insert cartridge, ACTION: zenity open ROM from files, label: Or drag and drop ROM

// TODO fix pause menu when starting game link connexion while pause menu is still active (it's working but weirdly so low priority)

// TODO rechceck mbc1/2/etc tests as i may have fixed some things

SDL_bool is_running = SDL_TRUE;
SDL_bool is_paused = SDL_FALSE;

SDL_Texture *ppu_texture;
SDL_AudioDeviceID audio_device;

void gbmulator_exit(void) {
    is_running = SDL_FALSE;
}

void gbmulator_unpause(void) {
    is_paused = SDL_FALSE;
}

int sdl_key_to_joypad(SDL_Keycode key) {
    switch (key) {
    case SDLK_RIGHT: return JOYPAD_RIGHT;
    case SDLK_LEFT: return JOYPAD_LEFT;
    case SDLK_UP: return JOYPAD_UP;
    case SDLK_DOWN: return JOYPAD_DOWN;
    case SDLK_KP_0: return JOYPAD_A;
    case SDLK_KP_PERIOD: return JOYPAD_B;
    case SDLK_KP_2: return JOYPAD_SELECT;
    case SDLK_KP_1: return JOYPAD_START;
    default: return key;
    }
}

static void ppu_vblank_cb(byte_t *pixels) {
    SDL_UpdateTexture(ppu_texture, NULL, pixels, 160 * sizeof(byte_t) * 3);
}

static void apu_samples_ready_cb(float *audio_buffer) {
    while (SDL_GetQueuedAudioSize(audio_device) > APU_SAMPLE_COUNT * sizeof(float))
        SDL_Delay(1);
    SDL_QueueAudio(audio_device, audio_buffer, sizeof(float) * APU_SAMPLE_COUNT);
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

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: %s path/to/rom.gb\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *config_path = load_config();

    // char *rom_title = mmu_load_cartridge(argv[1]);
    char *rom_title = mmu_load_cartridge("roms/tests/mooneye/emulator-only/mbc1/rom_4Mb.gb"); // floating point exception
    char window_title[sizeof(WINDOW_TITLE) + 19];
    snprintf(window_title, sizeof(window_title), WINDOW_TITLE" - %s", rom_title);
    printf("Playing %s\n", rom_title);

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

    ppu_set_vblank_callback(ppu_vblank_cb);
    apu_set_samples_ready_callback(apu_samples_ready_cb);

    byte_t *ui_pixels = ui_init();

    byte_t scale = config.scale;

    SDL_Window *window = SDL_CreateWindow(
        window_title,
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        160 * scale,
        144 * scale,
        SDL_WINDOW_HIDDEN /*| SDL_WINDOW_RESIZABLE*/
    );
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_ShowWindow(window); // show window after creating the renderer to avoid weird window show -> hide -> show at startup

    ppu_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, 160, 144);

    byte_t blank_pixel[3];
    SDL_Texture *blank_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STATIC, 1, 1);

    SDL_Texture *ui_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, 160, 144);
    SDL_SetTextureBlendMode(ui_texture, SDL_BLENDMODE_BLEND);

    SDL_AudioSpec AudioSettings = { 0 };
    AudioSettings.freq = APU_SAMPLE_RATE;
    AudioSettings.format = AUDIO_F32SYS;
    AudioSettings.channels = 2;
    AudioSettings.samples = APU_SAMPLE_COUNT;
    AudioSettings.callback = NULL;
    audio_device = SDL_OpenAudioDevice(NULL, 0, &AudioSettings, NULL, 0);
    SDL_PauseAudioDevice(audio_device, 0);

    // main gbmulator loop
    while (is_running) {
        handle_input();

        // emulation paused handling
        if (is_paused) {
            // display menu
            ui_draw_menu();

            if (scale != config.scale) {
                scale = config.scale;
                SDL_SetWindowSize(window, 160 * scale, 144 * scale);
            }

            // update ppu_texture to show color palette changes
            SDL_UpdateTexture(ppu_texture, NULL, ppu_get_pixels(), 160 * sizeof(byte_t) * 3);
            SDL_RenderCopy(renderer, ppu_texture, NULL, NULL);

            SDL_UpdateTexture(ui_texture, NULL, ui_pixels, 160 * sizeof(byte_t) * 4);
            SDL_RenderCopy(renderer, ui_texture, NULL, NULL);

            SDL_RenderPresent(renderer);
            // delay to aim 30 fps (and avoid 100% CPU use) here because the normal delay is done by the sound emulation which is paused
            SDL_Delay(1 / 30.0);
            continue;
        }

        // emulation loop
        emulator_run_cycles(CPU_CYCLES_PER_FRAME * config.speed);

        // draw last new frame if it's complete
        if (CHECK_BIT(mem[LCDC], 7)) {
            SDL_RenderCopy(renderer, ppu_texture, NULL, NULL);
        } else {
            // refresh blank_pixel's color in case of ppu palette change
            byte_t *white = ppu_get_color_values(WHITE);
            blank_pixel[0] = white[0];
            blank_pixel[1] = white[1];
            blank_pixel[2] = white[2];

            SDL_UpdateTexture(blank_texture, NULL, &blank_pixel, sizeof(byte_t) * 3);
            SDL_RenderCopy(renderer, blank_texture, NULL, NULL);
        }
        SDL_RenderPresent(renderer);

        // no delay at the end of the loop because it's redundant with vsync and sound emulation
    }

    mmu_save_eram();
    link_close_connection();
    
    save_config(config_path);

    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyTexture(ppu_texture);
    SDL_DestroyTexture(ui_texture);
    SDL_DestroyTexture(blank_texture);

    SDL_Quit();
    return EXIT_SUCCESS;
}
