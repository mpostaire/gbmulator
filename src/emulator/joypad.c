#include <stdlib.h>

#include "emulator.h"
#include "joypad.h"
#include "cpu.h"
#include "mmu.h"

void joypad_init(emulator_t *emu) {
    emu->joypad = xmalloc(sizeof(joypad_t));
    emu->joypad->action = 0xCF;
    emu->joypad->direction = 0xCF;
}

void joypad_quit(emulator_t *emu) {
    free(emu->joypad);
}

byte_t joypad_get_input(emulator_t *emu) {
    joypad_t *joypad = emu->joypad;
    mmu_t *mmu = emu->mmu;

    if (!CHECK_BIT(mmu->mem[P1], 4))
        return (mmu->mem[P1] & 0xF0) | joypad->direction;
    else if (!CHECK_BIT(mmu->mem[P1], 5))
        return (mmu->mem[P1] & 0xF0) | joypad->action;
    else if (!CHECK_BIT(mmu->mem[P1], 4) && (!CHECK_BIT(mmu->mem[P1], 5)))
        return (mmu->mem[P1] & 0xF0) | joypad->direction | joypad->action;
    else
        return 0xFF;
}

void joypad_press(emulator_t *emu, joypad_button_t key) {
    joypad_t *joypad = emu->joypad;
    mmu_t *mmu = emu->mmu;

    switch (key) {
    case JOYPAD_RIGHT:
        RESET_BIT(joypad->direction, 0);
        if (!CHECK_BIT(mmu->mem[P1], 4))
            cpu_request_interrupt(emu, IRQ_JOYPAD);
        break;
    case JOYPAD_LEFT:
        RESET_BIT(joypad->direction, 1);
        if (!CHECK_BIT(mmu->mem[P1], 4))
            cpu_request_interrupt(emu, IRQ_JOYPAD);
        break;
    case JOYPAD_UP:
        RESET_BIT(joypad->direction, 2);
        if (!CHECK_BIT(mmu->mem[P1], 4))
            cpu_request_interrupt(emu, IRQ_JOYPAD);
        break;
    case JOYPAD_DOWN:
        RESET_BIT(joypad->direction, 3);
        if (!CHECK_BIT(mmu->mem[P1], 4))
            cpu_request_interrupt(emu, IRQ_JOYPAD);
        break;
    case JOYPAD_A:
        RESET_BIT(joypad->action, 0);
        if (!CHECK_BIT(mmu->mem[P1], 5))
            cpu_request_interrupt(emu, IRQ_JOYPAD);
        break;
    case JOYPAD_B:
        RESET_BIT(joypad->action, 1);
        if (!CHECK_BIT(mmu->mem[P1], 5))
            cpu_request_interrupt(emu, IRQ_JOYPAD);
        break;
    case JOYPAD_SELECT:
        RESET_BIT(joypad->action, 2);
        if (!CHECK_BIT(mmu->mem[P1], 5))
            cpu_request_interrupt(emu, IRQ_JOYPAD);
        break;
    case JOYPAD_START:
        RESET_BIT(joypad->action, 3);
        if (!CHECK_BIT(mmu->mem[P1], 5))
            cpu_request_interrupt(emu, IRQ_JOYPAD);
        break;
    }
}

void joypad_release(emulator_t *emu, joypad_button_t key) {
    joypad_t *joypad = emu->joypad;

    switch (key) {
    case JOYPAD_RIGHT:
        SET_BIT(joypad->direction, 0);
        break;
    case JOYPAD_LEFT:
        SET_BIT(joypad->direction, 1);
        break;
    case JOYPAD_UP:
        SET_BIT(joypad->direction, 2);
        break;
    case JOYPAD_DOWN:
        SET_BIT(joypad->direction, 3);
        break;
    case JOYPAD_A:
        SET_BIT(joypad->action, 0);
        break;
    case JOYPAD_B:
        SET_BIT(joypad->action, 1);
        break;
    case JOYPAD_SELECT:
        SET_BIT(joypad->action, 2);
        break;
    case JOYPAD_START:
        SET_BIT(joypad->action, 3);
        break;
    }
}
