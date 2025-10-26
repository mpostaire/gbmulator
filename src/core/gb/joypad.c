#include <stdlib.h>

#include "gb_priv.h"

void joypad_reset(gb_t *gb) {
    memset(&gb->joypad, 0, sizeof(gb->joypad));
    gb->joypad.action    = 0x0F;
    gb->joypad.direction = 0x0F;
}

uint8_t joypad_get_input(gb_t *gb) {
    gb_joypad_t *joypad = &gb->joypad;
    gb_mmu_t    *mmu    = &gb->mmu;

    if (!CHECK_BIT(mmu->io_registers[IO_P1], 4))
        return 0xC0 | (mmu->io_registers[IO_P1] & 0xF0) | joypad->direction;
    else if (!CHECK_BIT(mmu->io_registers[IO_P1], 5))
        return 0xC0 | (mmu->io_registers[IO_P1] & 0xF0) | joypad->action;
    else if (!CHECK_BIT(mmu->io_registers[IO_P1], 4) && (!CHECK_BIT(mmu->io_registers[IO_P1], 5)))
        return 0xC0 | (mmu->io_registers[IO_P1] & 0xF0) | joypad->direction | joypad->action;
    else
        return 0xFF;
}

void joypad_press(gb_t *gb, gbmulator_joypad_t key) {
    gb_joypad_t *joypad = &gb->joypad;
    gb_mmu_t    *mmu    = &gb->mmu;

    switch (key) {
    case GBMULATOR_JOYPAD_RIGHT:
    case GBMULATOR_JOYPAD_LEFT:
    case GBMULATOR_JOYPAD_UP:
    case GBMULATOR_JOYPAD_DOWN:
        RESET_BIT(joypad->direction, key - 4);
        if (!CHECK_BIT(mmu->io_registers[IO_P1], 4))
            CPU_REQUEST_INTERRUPT(gb, IRQ_JOYPAD);
        break;
    case GBMULATOR_JOYPAD_A:
    case GBMULATOR_JOYPAD_B:
    case GBMULATOR_JOYPAD_SELECT:
    case GBMULATOR_JOYPAD_START:
        RESET_BIT(joypad->action, key);
        if (!CHECK_BIT(mmu->io_registers[IO_P1], 5))
            CPU_REQUEST_INTERRUPT(gb, IRQ_JOYPAD);
        break;
    default:
        break;
    }
}

void joypad_release(gb_t *gb, gbmulator_joypad_t key) {
    gb_joypad_t *joypad = &gb->joypad;

    switch (key) {
    case GBMULATOR_JOYPAD_RIGHT:
    case GBMULATOR_JOYPAD_LEFT:
    case GBMULATOR_JOYPAD_UP:
    case GBMULATOR_JOYPAD_DOWN:
        SET_BIT(joypad->direction, key - 4);
        break;
    case GBMULATOR_JOYPAD_A:
    case GBMULATOR_JOYPAD_B:
    case GBMULATOR_JOYPAD_SELECT:
    case GBMULATOR_JOYPAD_START:
        SET_BIT(joypad->action, key);
        break;
    default:
        break;
    }
}
