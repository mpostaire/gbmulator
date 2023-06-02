#pragma once

#include "types.h"
#include "serialize.h"

typedef enum {
	IRQ_VBLANK,
	IRQ_STAT,
	IRQ_TIMER,
	IRQ_SERIAL,
	IRQ_JOYPAD
} interrupt_t;

#define CPU_REQUEST_INTERRUPT(emu, irq) SET_BIT((emu)->mmu->mem[IF], (irq))
#define IS_DOUBLE_SPEED(emu) (((emu)->mmu->mem[KEY1] & 0x80) >> 7)

void cpu_step(emulator_t *emu);

void cpu_init(emulator_t *emu);

void cpu_quit(emulator_t *emu);

SERIALIZE_FUNCTION_DECLS(cpu);
