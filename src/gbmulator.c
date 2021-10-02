#include <stdlib.h>
#include <stdio.h>
#include <SDL2/SDL.h>

#include "types.h"
#include "mem.h"
#include "cpu.h"
#include "ppu.h"
#include "timer.h"

static void handle_input(SDL_KeyboardEvent *key) {
    if (key->type == SDL_KEYUP) {
        // printf("Key release: ");
        // TODO bits 4 and 5 are not set properly when key is released (it should reset only if no keys of its category are pressed?)
        //      MAYBE THIS PART IS TOTALLY WRONG the cpu should write to bits 4 and 5 depending on wich input it wants to read and the
        //       input bits change accordingly 
        switch (key->keysym.sym) {
        case SDLK_RIGHT:
            SET_BIT(mem[P1], 4); // select direction buttons
            SET_BIT(mem[P1], 0); // right
            break;
        case SDLK_LEFT:
            SET_BIT(mem[P1], 4); // select direction buttons
            SET_BIT(mem[P1], 1); // left
            break;
        case SDLK_DOWN:
            SET_BIT(mem[P1], 4); // select direction buttons
            SET_BIT(mem[P1], 3); // down
            break;
        case SDLK_UP:
            SET_BIT(mem[P1], 4); // select direction buttons
            SET_BIT(mem[P1], 2); // up
            break;
        case SDLK_a:
            SET_BIT(mem[P1], 5); // select action buttons
            SET_BIT(mem[P1], 0); // a
            break;
        case SDLK_b:
            SET_BIT(mem[P1], 5); // select action buttons
            SET_BIT(mem[P1], 1); // b
            break;
        case SDLK_RETURN:
            SET_BIT(mem[P1], 5); // select action buttons
            SET_BIT(mem[P1], 3); // start
            break;
        case SDLK_SPACE:
            SET_BIT(mem[P1], 5); // select action buttons
            SET_BIT(mem[P1], 2); // select
            break;
        case SDLK_n:
            cpu_step();
            break;
        }
    } else {
        // printf("Key press: ");
        switch (key->keysym.sym) {
        case SDLK_RIGHT:
            RESET_BIT(mem[P1], 4); // select direction buttons
            RESET_BIT(mem[P1], 0); // right
            cpu_request_interrupt(IRQ_JOYPAD);
            break;
        case SDLK_LEFT:
            RESET_BIT(mem[P1], 4); // select direction buttons
            RESET_BIT(mem[P1], 1); // left
            cpu_request_interrupt(IRQ_JOYPAD);
            break;
        case SDLK_DOWN:
            RESET_BIT(mem[P1], 4); // select direction buttons
            RESET_BIT(mem[P1], 3); // down
            cpu_request_interrupt(IRQ_JOYPAD);
            break;
        case SDLK_UP:
            RESET_BIT(mem[P1], 4); // select direction buttons
            RESET_BIT(mem[P1], 2); // up
            cpu_request_interrupt(IRQ_JOYPAD);
            break;
        case SDLK_a:
            RESET_BIT(mem[P1], 5); // select action buttons
            RESET_BIT(mem[P1], 0); // a
            cpu_request_interrupt(IRQ_JOYPAD);
            break;
        case SDLK_b:
            RESET_BIT(mem[P1], 5); // select action buttons
            RESET_BIT(mem[P1], 1); // b
            cpu_request_interrupt(IRQ_JOYPAD);
            break;
        case SDLK_RETURN:
            RESET_BIT(mem[P1], 5); // select action buttons
            RESET_BIT(mem[P1], 3); // start
            cpu_request_interrupt(IRQ_JOYPAD);
            break;
        case SDLK_SPACE:
            RESET_BIT(mem[P1], 5); // select action buttons
            RESET_BIT(mem[P1], 2); // select
            cpu_request_interrupt(IRQ_JOYPAD);
            break;
        }
    }
    // Print the hardware scancode and key name
    // printf("Scancode: 0x%02X, Name: %s\n", key->keysym.scancode, SDL_GetKeyName(key->keysym.sym));
}

int main(int argc, char **argv) {
    SDL_Init(SDL_INIT_VIDEO);

    // TODO add rom load cartdrige (bios.gbc) into memory

    SDL_Window *window = SDL_CreateWindow("gbmulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 160, 144, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, 160, 144);

    SDL_bool is_running = SDL_TRUE;
    const Uint32 fps = 60;
    const Uint32 frame_delay = 1000 / fps;
    Uint32 frame_time;
    SDL_Event event;

    // ALL CPU_INSTR TEST ROMS PASSED
    // load_cartridge("roms/tests/cpu_instrs/individual/01-special.gb");
    // load_cartridge("roms/tests/cpu_instrs/individual/02-interrupts.gb");
    // load_cartridge("roms/tests/cpu_instrs/individual/03-op sp,hl.gb");
    // load_cartridge("roms/tests/cpu_instrs/individual/04-op r,imm.gb");
    // load_cartridge("roms/tests/cpu_instrs/individual/05-op rp.gb");
    // load_cartridge("roms/tests/cpu_instrs/individual/06-ld r,r.gb");
    // load_cartridge("roms/tests/cpu_instrs/individual/07-jr,jp,call,ret,rst.gb");
    // load_cartridge("roms/tests/cpu_instrs/individual/08-misc instrs.gb");
    // load_cartridge("roms/tests/cpu_instrs/individual/09-op r,r.gb");
    // load_cartridge("roms/tests/cpu_instrs/individual/10-bit ops.gb");
    // load_cartridge("roms/tests/cpu_instrs/individual/11-op a,(hl).gb");

    load_cartridge("roms/Tetris.gb");

    long total_cycles = 0;
    while (is_running) {
        frame_time = SDL_GetTicks();

        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_KEYDOWN:
            case SDL_KEYUP:
                if (!event.key.repeat) 
                    handle_input(&event.key);
                break;
            case SDL_QUIT:
                is_running = SDL_FALSE;
                break;
            }
        }

        // 4194304 cycles executed per second --> 4194304 / fps --> 4194304 / 60 == 69905 cycles per frame
        int cycles_count = 0;
        while (cycles_count < 69905) {
            // if (instructions[mem[registers.pc]].operand_size == 0) {
            //     printf("A:%02x F:%c%c%c%c BC:%04x DE:%04x HL:%04x SP:%04x PC:%04x (cy: %ld) ppu:+0 |[00]0x0100 %02x\n", registers.a, CHECK_FLAG(FLAG_Z) ? 'Z' : '-', CHECK_FLAG(FLAG_N) ? 'N' : '-', CHECK_FLAG(FLAG_H) ? 'H' : '-', CHECK_FLAG(FLAG_C) ? 'C' : '-', registers.bc, registers.de, registers.hl, registers.sp, registers.pc, total_cycles, mem[registers.pc]);
            // } else if (instructions[mem[registers.pc]].operand_size == 1) {
            //     printf("A:%02x F:%c%c%c%c BC:%04x DE:%04x HL:%04x SP:%04x PC:%04x (cy: %ld) ppu:+0 |[00]0x0100 %02x %02x\n", registers.a, CHECK_FLAG(FLAG_Z) ? 'Z' : '-', CHECK_FLAG(FLAG_N) ? 'N' : '-', CHECK_FLAG(FLAG_H) ? 'H' : '-', CHECK_FLAG(FLAG_C) ? 'C' : '-', registers.bc, registers.de, registers.hl, registers.sp, registers.pc, total_cycles, mem[registers.pc], mem[registers.pc + 1]);
            // } else {
            //     printf("A:%02x F:%c%c%c%c BC:%04x DE:%04x HL:%04x SP:%04x PC:%04x (cy: %ld) ppu:+0 |[00]0x0100 %02x %02x %02x\n", registers.a, CHECK_FLAG(FLAG_Z) ? 'Z' : '-', CHECK_FLAG(FLAG_N) ? 'N' : '-', CHECK_FLAG(FLAG_H) ? 'H' : '-', CHECK_FLAG(FLAG_C) ? 'C' : '-', registers.bc, registers.de, registers.hl, registers.sp, registers.pc, total_cycles, mem[registers.pc], mem[registers.pc + 1], mem[registers.pc + 2]);
            // }
            // if (total_cycles >= 11828596) {
            //     exit(1);
            // }

            int cycles = cpu_handle_interrupts();
            cycles += cpu_step();
            timer_step(cycles);
            ppu_step(cycles, renderer, texture);

            cycles_count += cycles;
            total_cycles += cycles;
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
