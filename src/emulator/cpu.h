#pragma once

#include "types.h"

typedef enum {
	IRQ_VBLANK,
	IRQ_STAT,
	IRQ_TIMER,
	IRQ_SERIAL,
	IRQ_JOYPAD
} interrupt_t;

void cpu_request_interrupt(emulator_t *emu, int irq);

void cpu_step(emulator_t *emu);

void cpu_init(emulator_t *emu);

void cpu_quit(emulator_t *emu);
