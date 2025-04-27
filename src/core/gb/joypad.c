#include <stdlib.h>

#include "gb_priv.h"

void joypad_init(gb_t *gb) {
    gb->joypad = xcalloc(1, sizeof(*gb->joypad));
    gb->joypad->action = 0x0F;
    gb->joypad->direction = 0x0F;
}

void joypad_quit(gb_t *gb) {
    free(gb->joypad);
}

uint8_t joypad_get_input(gb_t *gb) {
    gb_joypad_t *joypad = gb->joypad;
    gb_mmu_t *mmu = gb->mmu;

    if (!CHECK_BIT(mmu->io_registers[IO_P1], 4))
        return 0xC0 | (mmu->io_registers[IO_P1] & 0xF0) | joypad->direction;
    else if (!CHECK_BIT(mmu->io_registers[IO_P1], 5))
        return 0xC0 | (mmu->io_registers[IO_P1] & 0xF0) | joypad->action;
    else if (!CHECK_BIT(mmu->io_registers[IO_P1], 4) && (!CHECK_BIT(mmu->io_registers[IO_P1], 5)))
        return 0xC0 | (mmu->io_registers[IO_P1] & 0xF0) | joypad->direction | joypad->action;
    else
        return 0xFF;
}

void joypad_press(gb_t *gb, joypad_button_t key) {
    gb_joypad_t *joypad = gb->joypad;
    gb_mmu_t *mmu = gb->mmu;

    switch (key) {
    case JOYPAD_RIGHT:
    case JOYPAD_LEFT:
    case JOYPAD_UP:
    case JOYPAD_DOWN:
        RESET_BIT(joypad->direction, key - 4);
        if (!CHECK_BIT(mmu->io_registers[IO_P1], 4))
            CPU_REQUEST_INTERRUPT(gb, IRQ_JOYPAD);
        break;
    case JOYPAD_A:
    case JOYPAD_B:
    case JOYPAD_SELECT:
    case JOYPAD_START:
        RESET_BIT(joypad->action, key);
        if (!CHECK_BIT(mmu->io_registers[IO_P1], 5))
            CPU_REQUEST_INTERRUPT(gb, IRQ_JOYPAD);
        break;
    default:
        break;
    }
}

void joypad_release(gb_t *gb, joypad_button_t key) {
    gb_joypad_t *joypad = gb->joypad;

    switch (key) {
    case JOYPAD_RIGHT:
    case JOYPAD_LEFT:
    case JOYPAD_UP:
    case JOYPAD_DOWN:
        SET_BIT(joypad->direction, key - 4);
        break;
    case JOYPAD_A:
    case JOYPAD_B:
    case JOYPAD_SELECT:
    case JOYPAD_START:
        SET_BIT(joypad->action, key);
        break;
    default:
        break;
    }
}
