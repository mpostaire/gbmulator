#pragma once

#include "gb.h"
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
            uint8_t f;
            uint8_t a;
        };
        uint16_t af;
    };

    union {
        struct {
            uint8_t c;
            uint8_t b;
        };
        uint16_t bc;
    };

    union {
        struct {
            uint8_t e;
            uint8_t d;
        };
        uint16_t de;
    };

    union {
        struct {
            uint8_t l;
            uint8_t h;
        };
        uint16_t hl;
    };

    uint16_t sp;
    uint16_t pc;
} gb_registers_t;

typedef struct {
    gb_registers_t registers;
    uint8_t ime; // interrupt master enable
    uint8_t halt;
    uint8_t halt_bug;
    uint8_t exec_state; // determines if the cpu is pushing an interrupt execution, executiong a normal opcode or a cb opcode
    uint8_t opcode; // current opcode
    int16_t opcode_state; // current opcode or current microcode inside the opcode (< 0 --> request new instruction fetch)
    uint16_t operand; // operand for the current opcode
    uint16_t accumulator; // storage used for an easier implementation of some opcodes
} gb_cpu_t;

#define CPU_REQUEST_INTERRUPT(gb, irq) SET_BIT((gb)->mmu->io_registers[IO_IF], (irq))
#define IS_DOUBLE_SPEED(gb) ((gb)->mmu->io_registers[IO_KEY1] >> 7)

void cpu_step(gb_t *gb);

void cpu_init(gb_t *gb);

void cpu_quit(gb_t *gb);

SERIALIZE_FUNCTION_DECLS(cpu);
