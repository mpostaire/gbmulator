#include <SDL2/SDL.h>

#include "joypad.h"
#include "cpu.h"
#include "mem.h"
#include "ppu.h"

byte_t joypad_action = 0x0F;
byte_t joypad_direction = 0x0F;

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
        return (mem[P1] & 0xF0) | joypad_direction;
    else if (!CHECK_BIT(mem[P1], 5))
        return (mem[P1] & 0xF0) | joypad_action;
    else if (!CHECK_BIT(mem[P1], 4) && (!CHECK_BIT(mem[P1], 5)))
        return (mem[P1] & 0xF0) | joypad_direction | joypad_action;
    else
        return 0xFF;
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
            if (!CHECK_BIT(mem[P1], 4))
                cpu_request_interrupt(IRQ_JOYPAD);
            break;
        case LEFT:
            RESET_BIT(joypad_direction, 1);
            if (!CHECK_BIT(mem[P1], 4))
                cpu_request_interrupt(IRQ_JOYPAD);
            break;
        case UP:
            RESET_BIT(joypad_direction, 2);
            if (!CHECK_BIT(mem[P1], 4))
                cpu_request_interrupt(IRQ_JOYPAD);
        case DOWN:
            RESET_BIT(joypad_direction, 3);
            if (!CHECK_BIT(mem[P1], 4))
                cpu_request_interrupt(IRQ_JOYPAD);
            break;
        case A:
            RESET_BIT(joypad_action, 0);
            if (!CHECK_BIT(mem[P1], 5))
                cpu_request_interrupt(IRQ_JOYPAD);
            break;
        case B:
            RESET_BIT(joypad_action, 1);
            if (!CHECK_BIT(mem[P1], 5))
                cpu_request_interrupt(IRQ_JOYPAD);
            break;
        case SELECT:
            RESET_BIT(joypad_action, 2);
            if (!CHECK_BIT(mem[P1], 5))
                cpu_request_interrupt(IRQ_JOYPAD);
            break;
        case START:
            RESET_BIT(joypad_action, 3);
            if (!CHECK_BIT(mem[P1], 5))
                cpu_request_interrupt(IRQ_JOYPAD);
            break;
        case SDLK_c:
            ppu_switch_colors();
            break;
        }
    }
}
