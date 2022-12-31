#pragma once

#include "types.h"

typedef enum {
	IRQ_VBLANK,
	IRQ_STAT,
	IRQ_TIMER,
	IRQ_SERIAL,
	IRQ_JOYPAD
} interrupt_t;

#define CPU_REQUEST_INTERRUPT(emu, irq) SET_BIT((emu)->mmu->mem[IF], (irq))

void cpu_step(emulator_t *emu);

void cpu_init(emulator_t *emu);

void cpu_quit(emulator_t *emu);

size_t cpu_serialized_length(emulator_t *emu);

byte_t *cpu_serialize(emulator_t *emu, size_t *size);

void cpu_unserialize(emulator_t *emu, byte_t *buf);
