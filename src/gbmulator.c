#include <stdlib.h>
#include <stdio.h>
#include <SDL2/SDL.h>

#include "types.h"
#include "mem.h"
#include "cpu.h"
#include "ppu.h"
#include "timer.h"
#include "joypad.h"

#define WINDOW_TITLE "gbmulator"
#define WINDOW_SCALE 3

int debug_vram = 0;
int paused = 0;

int main(int argc, char **argv) {
    // TODO program argument sets path to rom (optional: path to boot rom - default being roms/bios.gb)
    //      + print help, keybindings, etc...
    if (argc < 2) {
        printf("Usage: %s path/to/rom.gb\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    load_cartridge(argv[1]);

    // ALL CPU_INSTR TEST ROMS PASSED
    // load_cartridge("roms/tests/blargg/cpu_instrs/cpu_instrs.gb");
    // load_cartridge("roms/tests/blargg/instr_timing/instr_timing.gb");

    // load_cartridge("roms/tests/mooneye/emulator-only/mbc1/bits_mode.gb");
    // load_cartridge("roms/tests/mooneye/emulator-only/mbc2/bits_romb.gb");

    // load_cartridge("roms/Tetris.gb");
    // load_cartridge("roms/Super Mario Land.gb");
    // load_cartridge("roms/Dr. Mario.gb");
    // load_cartridge("roms/Pokemon Red.gb");

    SDL_Init(SDL_INIT_VIDEO);

    const int game_width =  160 * WINDOW_SCALE;
    const int game_height =  144 * WINDOW_SCALE;

    SDL_Window *window = SDL_CreateWindow(WINDOW_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, debug_vram ? game_width * 2 : game_width, game_height, SDL_WINDOW_SHOWN /*| SDL_WINDOW_RESIZABLE*/);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_Texture* game_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, 160, 144);
    SDL_Texture* vram_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, 160, 144);
    SDL_Rect game_rect = { .x = 0, .y = 0, .w = game_width, .h = game_height };
    SDL_Rect vram_rect = { .x = game_width, .y = 0, .w = game_width, .h = game_height };

    SDL_bool is_running = SDL_TRUE;
    const Uint32 fps = 60;
    const Uint32 frame_delay = 1000 / fps;

    while (is_running) {
        Uint32 frame_time = SDL_GetTicks();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_KEYDOWN:
                if (event.key.repeat)
                    break;
                joypad_press(event.key.keysym.sym);
                switch (event.key.keysym.sym) {
                case SDLK_c:
                    ppu_switch_colors();
                    break;
                case SDLK_d:
                    debug_vram = !debug_vram;
                    SDL_SetWindowSize(window, debug_vram ? game_width * 2 : game_width, game_height);
                    break;
                case SDLK_p:
                    paused = !paused;
                    SDL_SetWindowTitle(window, paused ? WINDOW_TITLE" - PAUSED" : WINDOW_TITLE);
                    break;
                }
                break;
            case SDL_KEYUP:
                if (!event.key.repeat)
                    joypad_release(event.key.keysym.sym);
                break;
            case SDL_QUIT:
                is_running = SDL_FALSE;
                break;
            }
        }

        // 4194304 cycles executed per second --> 4194304 / fps --> 4194304 / 60 == 69905 cycles per frame
        int cycles_count = 0;
        while (!paused && cycles_count < 69905) {
            // TODO make timings accurate by forcing each cpu_step() to take 4 cycles: if it's not enough to finish an instruction,
            // the next cpu_step() will resume the previous instruction. This will makes the timer "hack" (increment within a loop and not an if)
            // obsolete while allowing accurate memory timings emulation.
            int cycles = cpu_handle_interrupts();
            cycles += cpu_step();
            timer_step(cycles);
            byte_t *pixels = ppu_step(cycles);
            if (pixels) {
                SDL_UpdateTexture(game_texture, NULL, pixels, 160 * sizeof(byte_t) * 3);
                SDL_RenderCopy(renderer, game_texture, NULL, &game_rect);

                if (debug_vram) {
                    pixels = ppu_debug_oam();
                    SDL_UpdateTexture(vram_texture, NULL, pixels, 160 * sizeof(byte_t) * 3);
                    SDL_RenderCopy(renderer, vram_texture, NULL, &vram_rect);
                }

                SDL_RenderPresent(renderer);
            }

            cycles_count += cycles;
        }

        frame_time = SDL_GetTicks() - frame_time;
        if (frame_time < frame_delay) {
            SDL_Delay(frame_delay - frame_time);
        }
    }

    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);
    SDL_Quit();
    return EXIT_SUCCESS;
}
