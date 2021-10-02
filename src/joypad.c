#include <SDL2/SDL.h>

#include "joypad.h"
#include "cpu.h"
#include "mem.h"

byte_t joypad_action = 0xFF;
byte_t joypad_direction = 0xFF;

enum buttons {
    RIGHT = SDLK_RIGHT,
    LEFT = SDLK_LEFT,
    UP = SDLK_UP,
    DOWN = SDLK_DOWN,
    A = SDLK_a,
    B = SDLK_b,
    SELECT = SDLK_SPACE,
    START = SDLK_RETURN
};

byte_t joypad_get_input(void) {
    if (!CHECK_BIT(mem[P1], 4))
        return joypad_direction;
    else
        return joypad_action;
}

void joypad_update(SDL_KeyboardEvent *key) {
    if (key->type == SDL_KEYUP) {
        switch (key->keysym.sym) {
        case RIGHT:
            SET_BIT(joypad_direction, 0);
            break;
        case LEFT:
            SET_BIT(joypad_direction, 1);
            break;
        case UP:
            SET_BIT(joypad_direction, 2);
            break;
        case DOWN:
            SET_BIT(joypad_direction, 3);
            break;
        case A:
            SET_BIT(joypad_action, 0);
            break;
        case B:
            SET_BIT(joypad_action, 1);
            break;
        case SELECT:
            SET_BIT(joypad_action, 2);
            break;
        case START:
            SET_BIT(joypad_action, 3);
            break;
        }
    } else {
        switch (key->keysym.sym) {
        case RIGHT:
            RESET_BIT(joypad_direction, 0);
            cpu_request_interrupt(IRQ_JOYPAD);
            break;
        case LEFT:
            RESET_BIT(joypad_direction, 1);
            cpu_request_interrupt(IRQ_JOYPAD);
            break;
        case UP:
            RESET_BIT(joypad_direction, 2);
            cpu_request_interrupt(IRQ_JOYPAD);
        case DOWN:
            RESET_BIT(joypad_direction, 3);
            cpu_request_interrupt(IRQ_JOYPAD);
            break;
            break;
        case A:
            RESET_BIT(joypad_action, 0);
            cpu_request_interrupt(IRQ_JOYPAD);
            break;
        case B:
            RESET_BIT(joypad_action, 1);
            cpu_request_interrupt(IRQ_JOYPAD);
            break;
        case SELECT:
            RESET_BIT(joypad_action, 2);
            cpu_request_interrupt(IRQ_JOYPAD);
            break;
        case START:
            RESET_BIT(joypad_action, 3);
            cpu_request_interrupt(IRQ_JOYPAD);
            break;
        case SDLK_n:
            cpu_step();
            break;
        case SDLK_c:
            ppu_switch_colors();
            break;
        }
    }
}
