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
    byte_t halt_bug;
    byte_t exec_state; // determines if the cpu is pushing an interrupt execution, executiong a normal opcode or a cb opcode
    byte_t opcode; // current opcode
    s_word_t opcode_state; // current opcode or current microcode inside the opcode (< 0 --> request new instruction fetch)
    word_t operand; // operand for the current opcode
    word_t opcode_cache_variable; // storage used for an easier implementation of some opcodes
} cpu_t;

#define CPU_REQUEST_INTERRUPT(emu, irq) SET_BIT((emu)->mmu->mem[IF], (irq))
#define IS_DOUBLE_SPEED(emu) (((emu)->mmu->mem[KEY1] & 0x80) >> 7)

void cpu_step(emulator_t *emu);

void cpu_init(emulator_t *emu);

void cpu_quit(emulator_t *emu);

SERIALIZE_FUNCTION_DECLS(cpu);
