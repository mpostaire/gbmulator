#include <stdlib.h>
#include <stdio.h>
#include <SDL2/SDL.h>

#include "types.h"
#include "mem.h"
#include "cpu.h"
#include "ppu.h"
#include "timer.h"
#include "joypad.h"
#include "serial.h"
#include "apu.h"

#define WINDOW_TITLE "gbmulator"
#define WINDOW_SCALE 3
#define MAX_SPEED 4

// FIXME super mario land no longer works properly --> LY==LYC interrupt fix commit is responsible (check argentum emulator ppu code to see how its handled)

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

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    apu_init();

    SDL_Window *window = SDL_CreateWindow(WINDOW_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 160 * WINDOW_SCALE, 144 * WINDOW_SCALE, SDL_WINDOW_SHOWN /*| SDL_WINDOW_RESIZABLE*/);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, 160, 144);
    SDL_Texture* blank_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STATIC, 1, 1);

    SDL_bool is_running = SDL_TRUE;
    // 4194304 cycles executed per second --> 4194304 / fps --> 4194304 / 60 == 69905 cycles per frame
    const Uint32 cycles_per_frame = 4194304 / 60;

    SDL_bool paused = 0;
    float speed = 1.0f;

    printf("Emulation speed: %fx\n", speed);

    while (is_running) {
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
                case SDLK_p:
                    paused = !paused;
                    SDL_SetWindowTitle(window, paused ? WINDOW_TITLE" - PAUSED" : WINDOW_TITLE);
                    break;
                case SDLK_s:
                    serial_start_server(7777);
                    break;
                case SDLK_d:
                    serial_connect_to_server("127.0.0.1", 7777);
                    break;
                case SDLK_m:
                    speed += 0.5f;
                    if (speed > MAX_SPEED)
                        speed = 1.0f;
                    printf("Emulation speed: %fx\n", speed);
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

        int cycles_count = 0;
        while (!paused && cycles_count < cycles_per_frame * speed) {
            // TODO make timings accurate by forcing each cpu_step() to take 4 cycles: if it's not enough to finish an instruction,
            // the next cpu_step() will resume the previous instruction. This will makes the timer "hack" (increment within a loop and not an if)
            // obsolete while allowing accurate memory timings emulation.
            int cycles = cpu_handle_interrupts();
            cycles += cpu_step();
            timer_step(cycles);
            serial_step(cycles);
            SDL_bool draw_frame = ppu_step(cycles);
            if (draw_frame)
                SDL_UpdateTexture(texture, NULL, pixels, 160 * sizeof(byte_t) * 3);

            apu_step(cycles, speed);
            cycles_count += cycles;
        }

        // draw last new frame if it's complete
        if (CHECK_BIT(mem[LCDC], 7)) {
            SDL_RenderCopy(renderer, texture, NULL, NULL);
        } else {
            SET_PIXEL(blank_pixel, 0, 0, WHITE); // SET_PIXEL here to refresh color in case of color change
            SDL_UpdateTexture(blank_texture, NULL, &blank_pixel, sizeof(byte_t) * 3);
            SDL_RenderCopy(renderer, blank_texture, NULL, NULL);
        }
        SDL_RenderPresent(renderer);

        // no delay at the end of the loop because it's redundant with vsync
    }

    mem_save_eram();
    serial_close_connection();

    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);
    SDL_Quit();
    return EXIT_SUCCESS;
}
