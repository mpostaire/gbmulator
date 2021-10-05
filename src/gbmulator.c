#include <stdlib.h>
#include <stdio.h>
#include <SDL2/SDL.h>

#include "types.h"
#include "mem.h"
#include "cpu.h"
#include "ppu.h"
#include "timer.h"
#include "joypad.h"

#define WINDOW_SCALE 3

#include "debug.h"
long total_cycles = 0;
int debug = 0;

int main(int argc, char **argv) {
    // TODO not all characters are displayed for these test roms...

    // ALL CPU_INSTR TEST ROMS PASSED
    // load_cartridge("roms/tests/blargg/cpu_instrs/cpu_instrs.gb");
    // load_cartridge("roms/tests/blargg/instr_timing/instr_timing.gb");

    // load_cartridge("roms/tests/mooneye/emulator-only/mbc1/rom_512kb.gb");
    // load_cartridge("roms/tests/mooneye/emulator-only/mbc1/bits_ramg.gb");

    // TODO program argument sets path to rom (optional: path to boot rom - default being roms/bios.gb)
    // load_cartridge("roms/Tetris.gb"); // up button press read as down and never releases until down is pressed (also in super mario land)
    // load_cartridge("roms/Dr. Mario.gb");
    load_cartridge("roms/Super Mario Land.gb"); // sprite not rendered if next to left border (test right)

    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window *window = SDL_CreateWindow("gbmulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 160 * WINDOW_SCALE, 144 * WINDOW_SCALE, SDL_WINDOW_SHOWN /*| SDL_WINDOW_RESIZABLE*/);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, 160, 144);

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
                // if (event.key.keysym.sym == SDLK_RETURN)
                //     debug++;
                if (event.key.keysym.sym == SDLK_c)
                    ppu_switch_colors();
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
        while (cycles_count < 69905) {
            if (debug)
                print_trace();

            // TODO make timings accurate by forcing each cpu_step() to take 4 cycles: if it's not enough to finish an instruction,
            // the next cpu_step() will resume the previous instruction. This will makes the timer "hack" (increment within a loop and not an if)
            // obsolete while allowing accurate memory timings emulation.
            int cycles = cpu_handle_interrupts();
            cycles += cpu_step();
            timer_step(cycles);
            ppu_step(cycles, renderer, texture);

            total_cycles += cycles;
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
