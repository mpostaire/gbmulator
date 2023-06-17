#include <stdlib.h>

#include "emulator_priv.h"

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
        return 0xC0 | (mmu->mem[P1] & 0xF0) | joypad->direction;
    else if (!CHECK_BIT(mmu->mem[P1], 5))
        return 0xC0 | (mmu->mem[P1] & 0xF0) | joypad->action;
    else if (!CHECK_BIT(mmu->mem[P1], 4) && (!CHECK_BIT(mmu->mem[P1], 5)))
        return 0xC0 | (mmu->mem[P1] & 0xF0) | joypad->direction | joypad->action;
    else
        return 0xFF;
}

void joypad_press(emulator_t *emu, joypad_button_t key) {
    joypad_t *joypad = emu->joypad;
    mmu_t *mmu = emu->mmu;

    switch (key) {
    case JOYPAD_RIGHT:
    case JOYPAD_LEFT:
    case JOYPAD_UP:
    case JOYPAD_DOWN:
        RESET_BIT(joypad->direction, key);
        if (!CHECK_BIT(mmu->mem[P1], 4))
            CPU_REQUEST_INTERRUPT(emu, IRQ_JOYPAD);
        break;
    case JOYPAD_A:
    case JOYPAD_B:
    case JOYPAD_SELECT:
    case JOYPAD_START:
        RESET_BIT(joypad->action, key - 4);
        if (!CHECK_BIT(mmu->mem[P1], 5))
            CPU_REQUEST_INTERRUPT(emu, IRQ_JOYPAD);
        break;
    }
}

void joypad_release(emulator_t *emu, joypad_button_t key) {
    joypad_t *joypad = emu->joypad;

    switch (key) {
    case JOYPAD_RIGHT:
    case JOYPAD_LEFT:
    case JOYPAD_UP:
    case JOYPAD_DOWN:
        SET_BIT(joypad->direction, key);
        break;
    case JOYPAD_A:
    case JOYPAD_B:
    case JOYPAD_SELECT:
    case JOYPAD_START:
        SET_BIT(joypad->action, key - 4);
        break;
    }
}
