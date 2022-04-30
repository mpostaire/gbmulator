#include <stdlib.h>
#include <stdio.h>
#include <SDL2/SDL.h>

#include "types.h"
#include "utils.h"
#include "mem.h"
#include "cpu.h"
#include "ppu.h"
#include "timer.h"
#include "joypad.h"
#include "link.h"
#include "apu.h"
#include "ui.h"
#include "config.h"

#define WINDOW_TITLE "GBmulator"

// FIXME super mario land no longer works properly --> LY==LYC interrupt fix commit is responsible (check argentum emulator ppu code to see how its handled)

// TODO when no argument, show alternative menu with title Insert cartridge, ACTION: zenity open ROM from files, label: Or drag and drop ROM

// TODO fix pause menu when starting game link connexion while pause menu is still active (it's working but weirdly so low priority)

// TODO create subdir containing the emulation files/code and keep main/ui/config/utils separate inside the root src dir
//      separate the emulation code from the main/ui/config/utils by making appropriate getter/setter/etc.

SDL_bool is_running = SDL_TRUE;
SDL_bool is_paused = SDL_FALSE;

void gbmulator_exit(void) {
    is_running = SDL_FALSE;
}

void gbmulator_unpause(void) {
    is_paused = SDL_FALSE;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: %s path/to/rom.gb\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *config_path = load_config();

    char *rom_title = mem_load_cartridge(argv[1]);
    char window_title[sizeof(WINDOW_TITLE) + 19];
    snprintf(window_title, sizeof(window_title), WINDOW_TITLE" - %s", rom_title);
    printf("Playing %s\n", rom_title);

    // ALL CPU_INSTR TEST ROMS PASSED
    // mem_load_cartridge("roms/tests/blargg/cpu_instrs/cpu_instrs.gb");
    // mem_load_cartridge("roms/tests/blargg/instr_timing/instr_timing.gb");

    // mem_load_cartridge("roms/tests/mooneye/emulator-only/mbc1/bits_mode.gb");
    // mem_load_cartridge("roms/tests/mooneye/emulator-only/mbc2/bits_romb.gb");

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    apu_init();
    ui_init();

    byte_t scale = config.scale;

    SDL_Window *window = SDL_CreateWindow(
        window_title,
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        160 * scale,
        144 * scale,
        SDL_WINDOW_SHOWN /*| SDL_WINDOW_RESIZABLE*/
    );
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, 160, 144);

    byte_t blank_pixel[3];
    SDL_Texture *blank_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STATIC, 1, 1);

    SDL_Texture *overlay_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, 160, 144);
    SDL_SetTextureBlendMode(overlay_texture, SDL_BLENDMODE_BLEND);

    // 4194304 cycles executed per second --> 4194304 / fps --> 4194304 / 60 == 69905 cycles per frame
    const Uint32 cycles_per_frame = 4194304 / 60;

    // main gbmulator loop
    while (is_running) {
        // input handling
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
                joypad_press(event.key.keysym.sym);
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

        // emulation paused handling
        if (is_paused) {
            // display menu
            ui_draw_menu();

            if (scale != config.scale) {
                scale = config.scale;
                SDL_SetWindowSize(window, 160 * scale, 144 * scale);
            }

            // update main texture to show color palette changes
            SDL_UpdateTexture(texture, NULL, pixels, 160 * sizeof(byte_t) * 3);
            SDL_RenderCopy(renderer, texture, NULL, NULL);

            // update overlay (menu) texture
            SDL_UpdateTexture(overlay_texture, NULL, ui_pixels, 160 * sizeof(byte_t) * 4);
            SDL_RenderCopy(renderer, overlay_texture, NULL, NULL);

            SDL_RenderPresent(renderer);
            // delay to aim 30 fps (and avoid 100% CPU use) here because the normal delay is done by the sound emulation which is paused
            SDL_Delay(1 / 30.0);
            continue;
        }

        // emulation loop
        int cycles_count = 0;
        while (cycles_count < cycles_per_frame * config.speed) {
            // TODO make timings accurate by forcing each cpu_step() to take 4 cycles: if it's not enough to finish an instruction,
            // the next cpu_step() will resume the previous instruction. This will makes the timer "hack" (increment within a loop and not an if)
            // obsolete while allowing accurate memory timings emulation.
            int cycles = cpu_handle_interrupts();
            cycles += cpu_step();
            timer_step(cycles);
            link_step(cycles);
            SDL_bool draw_frame = ppu_step(cycles);
            if (draw_frame)
                SDL_UpdateTexture(texture, NULL, pixels, 160 * sizeof(byte_t) * 3);

            apu_step(cycles, config.speed);
            cycles_count += cycles;
        }

        // draw last new frame if it's complete
        if (CHECK_BIT(mem[LCDC], 7)) {
            SDL_RenderCopy(renderer, texture, NULL, NULL);
        } else {
            SET_PIXEL(blank_pixel, 0, 0, WHITE); // SET_PIXEL here to refresh color in case of emulation color change
            SDL_UpdateTexture(blank_texture, NULL, &blank_pixel, sizeof(byte_t) * 3);
            SDL_RenderCopy(renderer, blank_texture, NULL, NULL);
        }
        SDL_RenderPresent(renderer);

        // no delay at the end of the loop because it's redundant with vsync and sound emulation
    }

    mem_save_eram();
    link_close_connection();
    
    save_config(config_path);

    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);
    SDL_Quit();
    return EXIT_SUCCESS;
}
