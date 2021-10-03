#include <stdlib.h>
#include <stdio.h>
#include <SDL2/SDL.h>

#include "types.h"
#include "mem.h"
#include "cpu.h"
#include "ppu.h"
#include "timer.h"
#include "joypad.h"

int main(int argc, char **argv) {
    // TODO not all characters are displayed for these test roms...
    //      check if its because ppu window is badly implemented (unlikely) or because last characters are objects (not actually implemented)

    // ALL CPU_INSTR TEST ROMS PASSED
    // load_cartridge("roms/tests/blargg/cpu_instrs/individual/01-special.gb");
    // load_cartridge("roms/tests/blargg/cpu_instrs/individual/02-interrupts.gb");
    // load_cartridge("roms/tests/blargg/cpu_instrs/individual/03-op sp,hl.gb");
    // load_cartridge("roms/tests/blargg/cpu_instrs/individual/04-op r,imm.gb");
    // load_cartridge("roms/tests/blargg/cpu_instrs/individual/05-op rp.gb");
    // load_cartridge("roms/tests/blargg/cpu_instrs/individual/06-ld r,r.gb");
    // load_cartridge("roms/tests/blargg/cpu_instrs/individual/07-jr,jp,call,ret,rst.gb");
    // load_cartridge("roms/tests/blargg/cpu_instrs/individual/08-misc instrs.gb");
    // load_cartridge("roms/tests/blargg/cpu_instrs/individual/09-op r,r.gb");
    // load_cartridge("roms/tests/blargg/cpu_instrs/individual/10-bit ops.gb");
    // load_cartridge("roms/tests/blargg/cpu_instrs/individual/11-op a,(hl).gb");
    // load_cartridge("roms/tests/blargg/instr_timing/instr_timing.gb");

    // TODO program argument sets path to rom (optional: path to boot rom - default being roms/bios.gb)
    load_cartridge("roms/Tetris.gb");

    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window *window = SDL_CreateWindow("gbmulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 160, 144, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
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
            // TODO make timings accurate by forcing each cpu_step() to take 4 cycles: if it's not enough to finish an instruction,
            // the next cpu_step() will resume the previous instruction. This will makes the timer "hack" (increment within a loop and not an if)
            // obsolete while allowing accurate memory timings emulation.
            int cycles = cpu_handle_interrupts();
            cycles += cpu_step();
            timer_step(cycles);
            ppu_step(cycles, renderer, texture);

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
