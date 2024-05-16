#pragma once

#include "types.h"
#include "serialize.h"

typedef enum {
	IRQ_VBLANK,
	IRQ_STAT,
	IRQ_TIMER,
	IRQ_SERIAL,
	IRQ_JOYPAD
} gb_interrupt_t;

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
} gb_registers_t;

typedef struct {
    gb_registers_t registers;
    byte_t ime; // interrupt master enable
    byte_t halt;
    byte_t halt_bug;
    byte_t exec_state; // determines if the cpu is pushing an interrupt execution, executiong a normal opcode or a cb opcode
    byte_t opcode; // current opcode
    s_word_t opcode_state; // current opcode or current microcode inside the opcode (< 0 --> request new instruction fetch)
    word_t operand; // operand for the current opcode
    word_t accumulator; // storage used for an easier implementation of some opcodes
} gb_cpu_t;

#define CPU_REQUEST_INTERRUPT(gb, irq) SET_BIT((gb)->mmu->io_registers[IO_IF], (irq))
#define IS_DOUBLE_SPEED(gb) ((gb)->mmu->io_registers[IO_KEY1] >> 7)

void cpu_step(gb_t *gb);

void cpu_init(gb_t *gb);

void cpu_quit(gb_t *gb);

SERIALIZE_FUNCTION_DECLS(cpu);
