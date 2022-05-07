#pragma once

#include "types.h"

#define FLAG_Z 0x80 // flag zero
#define FLAG_N 0x40 // flag substraction
#define FLAG_H 0x20 // flag half carry
#define FLAG_C 0x10 // flag carry

#define SET_FLAG(x) (cpu.registers.f |= (x))
#define CHECK_FLAG(x) (cpu.registers.f & (x))
#define RESET_FLAG(x) (cpu.registers.f &= ~(x))

typedef enum {
	IRQ_VBLANK,
	IRQ_STAT,
	IRQ_TIMER,
	IRQ_SERIAL,
	IRQ_JOYPAD
} interrupt_t;

typedef struct {
	union {
		struct {
			byte_t f;
			byte_t a;
		};
		word_t af;
	};

	union {
		struct {
			byte_t c;
			byte_t b;
		};
		word_t bc;
	};

	union {
		struct {
			byte_t e;
			byte_t d;
		};
		word_t de;
	};

	union {
		struct {
			byte_t l;
			byte_t h;
		};
		word_t hl;
	};

	word_t sp;
	word_t pc;
} registers_t;

typedef struct {
	registers_t registers;
	byte_t ime; // interrupt master enable
	byte_t halt;
} cpu_t;

extern cpu_t cpu;

void cpu_request_interrupt(int irq);

int cpu_step(void);

void cpu_init(void);
