#include "joypad.h"
#include "cpu.h"
#include "mmu.h"
#include "utils.h"
#include "types.h"

byte_t joypad_action = 0xCF;
byte_t joypad_direction = 0xCF;

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

void joypad_press(enum joypad_button key) {
    switch (key) {
    case JOYPAD_RIGHT:
        RESET_BIT(joypad_direction, 0);
        if (!CHECK_BIT(mem[P1], 4))
            cpu_request_interrupt(IRQ_JOYPAD);
        break;
    case JOYPAD_LEFT:
        RESET_BIT(joypad_direction, 1);
        if (!CHECK_BIT(mem[P1], 4))
            cpu_request_interrupt(IRQ_JOYPAD);
        break;
    case JOYPAD_UP:
        RESET_BIT(joypad_direction, 2);
        if (!CHECK_BIT(mem[P1], 4))
            cpu_request_interrupt(IRQ_JOYPAD);
        break;
    case JOYPAD_DOWN:
        RESET_BIT(joypad_direction, 3);
        if (!CHECK_BIT(mem[P1], 4))
            cpu_request_interrupt(IRQ_JOYPAD);
        break;
    case JOYPAD_A:
        RESET_BIT(joypad_action, 0);
        if (!CHECK_BIT(mem[P1], 5))
            cpu_request_interrupt(IRQ_JOYPAD);
        break;
    case JOYPAD_B:
        RESET_BIT(joypad_action, 1);
        if (!CHECK_BIT(mem[P1], 5))
            cpu_request_interrupt(IRQ_JOYPAD);
        break;
    case JOYPAD_SELECT:
        RESET_BIT(joypad_action, 2);
        if (!CHECK_BIT(mem[P1], 5))
            cpu_request_interrupt(IRQ_JOYPAD);
        break;
    case JOYPAD_START:
        RESET_BIT(joypad_action, 3);
        if (!CHECK_BIT(mem[P1], 5))
            cpu_request_interrupt(IRQ_JOYPAD);
        break;
    }
}

void joypad_release(enum joypad_button key) {
    switch (key) {
    case JOYPAD_RIGHT:
        SET_BIT(joypad_direction, 0);
        break;
    case JOYPAD_LEFT:
        SET_BIT(joypad_direction, 1);
        break;
    case JOYPAD_UP:
        SET_BIT(joypad_direction, 2);
        break;
    case JOYPAD_DOWN:
        SET_BIT(joypad_direction, 3);
        break;
    case JOYPAD_A:
        SET_BIT(joypad_action, 0);
        break;
    case JOYPAD_B:
        SET_BIT(joypad_action, 1);
        break;
    case JOYPAD_SELECT:
        SET_BIT(joypad_action, 2);
        break;
    case JOYPAD_START:
        SET_BIT(joypad_action, 3);
        break;
    }
}