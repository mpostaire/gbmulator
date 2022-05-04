#pragma once

#include "types.h"

#define CPU_FREQ 4194304
// 4194304 cycles executed per second --> 4194304 / fps --> 4194304 / 60 == 69905 cycles per frame (the Game Boy runs at approximatively 60 fps)
#define CPU_CYCLES_PER_FRAME CPU_FREQ / 60

#define FLAG_Z 0x80 // flag zero
#define FLAG_N 0x40 // flag substraction
#define FLAG_H 0x20 // flag half carry
#define FLAG_C 0x10 // flag carry

#define SET_FLAG(x) (registers.f |= (x))
#define CHECK_FLAG(x) (registers.f & (x))
#define RESET_FLAG(x) (registers.f &= ~(x))

enum interrupt {
	IRQ_VBLANK,
	IRQ_STAT,
	IRQ_TIMER,
	IRQ_SERIAL,
	IRQ_JOYPAD
};

struct registers {
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
} extern registers;

extern byte_t cpu_ime; // interrupt master enable
extern byte_t cpu_halt;

void cpu_request_interrupt(int irq);

int cpu_step(void);
