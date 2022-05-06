#include "joypad.h"
#include "cpu.h"
#include "mmu.h"
#include "utils.h"
#include "types.h"

struct joypad_t {
    byte_t joypad_action;
    byte_t joypad_direction;
} joypad;

void joypad_init(void) {
    joypad = (struct joypad_t) {
        .joypad_action = 0xCF,
        .joypad_direction = 0xCF,
    };
}

byte_t joypad_get_input(void) {
    if (!CHECK_BIT(mmu.mem[P1], 4))
        return (mmu.mem[P1] & 0xF0) | joypad.joypad_direction;
    else if (!CHECK_BIT(mmu.mem[P1], 5))
        return (mmu.mem[P1] & 0xF0) | joypad.joypad_action;
    else if (!CHECK_BIT(mmu.mem[P1], 4) && (!CHECK_BIT(mmu.mem[P1], 5)))
        return (mmu.mem[P1] & 0xF0) | joypad.joypad_direction | joypad.joypad_action;
    else
        return 0xFF;
}

void joypad_press(joypad_button_t key) {
    switch (key) {
    case JOYPAD_RIGHT:
        RESET_BIT(joypad.joypad_direction, 0);
        if (!CHECK_BIT(mmu.mem[P1], 4))
            cpu_request_interrupt(IRQ_JOYPAD);
        break;
    case JOYPAD_LEFT:
        RESET_BIT(joypad.joypad_direction, 1);
        if (!CHECK_BIT(mmu.mem[P1], 4))
            cpu_request_interrupt(IRQ_JOYPAD);
        break;
    case JOYPAD_UP:
        RESET_BIT(joypad.joypad_direction, 2);
        if (!CHECK_BIT(mmu.mem[P1], 4))
            cpu_request_interrupt(IRQ_JOYPAD);
        break;
    case JOYPAD_DOWN:
        RESET_BIT(joypad.joypad_direction, 3);
        if (!CHECK_BIT(mmu.mem[P1], 4))
            cpu_request_interrupt(IRQ_JOYPAD);
        break;
    case JOYPAD_A:
        RESET_BIT(joypad.joypad_action, 0);
        if (!CHECK_BIT(mmu.mem[P1], 5))
            cpu_request_interrupt(IRQ_JOYPAD);
        break;
    case JOYPAD_B:
        RESET_BIT(joypad.joypad_action, 1);
        if (!CHECK_BIT(mmu.mem[P1], 5))
            cpu_request_interrupt(IRQ_JOYPAD);
        break;
    case JOYPAD_SELECT:
        RESET_BIT(joypad.joypad_action, 2);
        if (!CHECK_BIT(mmu.mem[P1], 5))
            cpu_request_interrupt(IRQ_JOYPAD);
        break;
    case JOYPAD_START:
        RESET_BIT(joypad.joypad_action, 3);
        if (!CHECK_BIT(mmu.mem[P1], 5))
            cpu_request_interrupt(IRQ_JOYPAD);
        break;
    }
}

void joypad_release(joypad_button_t key) {
    switch (key) {
    case JOYPAD_RIGHT:
        SET_BIT(joypad.joypad_direction, 0);
        break;
    case JOYPAD_LEFT:
        SET_BIT(joypad.joypad_direction, 1);
        break;
    case JOYPAD_UP:
        SET_BIT(joypad.joypad_direction, 2);
        break;
    case JOYPAD_DOWN:
        SET_BIT(joypad.joypad_direction, 3);
        break;
    case JOYPAD_A:
        SET_BIT(joypad.joypad_action, 0);
        break;
    case JOYPAD_B:
        SET_BIT(joypad.joypad_action, 1);
        break;
    case JOYPAD_SELECT:
        SET_BIT(joypad.joypad_action, 2);
        break;
    case JOYPAD_START:
        SET_BIT(joypad.joypad_action, 3);
        break;
    }
}
