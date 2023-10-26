#include <stdio.h>
#include <stdlib.h>

#include "gb_priv.h"

#define FLAG_Z 0x80 // flag zero
#define FLAG_N 0x40 // flag substraction
#define FLAG_H 0x20 // flag half carry
#define FLAG_C 0x10 // flag carry

#define SET_FLAG(cpu, x) ((cpu)->registers.f |= (x))
#define CHECK_FLAG(cpu, x) ((cpu)->registers.f & (x))
#define RESET_FLAG(cpu, x) ((cpu)->registers.f &= ~(x))

#define PREPARE_SPEED_SWITCH(gb) ((gb)->mmu->io_registers[KEY1 - IO] & 0x01)
#define ENABLE_DOUBLE_SPEED(gb) do { ((gb)->mmu->io_registers[KEY1 - IO] |= 0x80); ((gb)->mmu->io_registers[KEY1 - IO] &= 0xFE); } while (0)
#define DISABLE_DOUBLE_SPEED(gb) do { ((gb)->mmu->io_registers[KEY1 - IO] &= 0x7F); ((gb)->mmu->io_registers[KEY1 - IO] &= 0xFE); } while (0)

#define IS_INTERRUPT_PENDING(gb) ((gb)->mmu->ie & (gb)->mmu->io_registers[IF - IO] & 0x1F)

typedef enum {
    IME_DISABLED,
    IME_PENDING,
    IME_ENABLED
} ime_state;

typedef enum {
    FETCH_OPCODE,
    FETCH_OPCODE_CB,
    EXEC_OPCODE,
    EXEC_OPCODE_CB,
    EXEC_PUSH_IRQ
} exec_state;

// must be used in the last microcode (CLOCK() call) of an opcode
#define END_OPCODE cpu->exec_state = FETCH_OPCODE
#define END_PUSH_IRQ { END_OPCODE; cpu->ime = IME_DISABLED; }
#define START_OPCODE_CB cpu->exec_state = FETCH_OPCODE_CB

// https://www.reddit.com/r/EmuDev/comments/a7kr9h/comment/ec3wkfo/?utm_source=share&utm_medium=web2x&context=3
#define _CLOCK(_label, ...)             \
    {                                   \
    case _label:                        \
        cpu->opcode_state = _label + 1; \
        __VA_ARGS__;                    \
        break;                          \
    }

// takes 4 cycles
#define CLOCK(...) _CLOCK((0x0400 + __COUNTER__), __VA_ARGS__)

typedef struct {
    char *name;
    int operand_size;
} opcode_t;

const opcode_t instructions[256] = {
    {"NOP", 0},                   // 0x00
    {"LD BC, %04X", 2},           // 0x01
    {"LD (BC), A", 0},            // 0x02
    {"INC BC", 0},                // 0x03
    {"INC B", 0},                 // 0x04
    {"DEC B", 0},                 // 0x05
    {"LD B, %02X", 1},            // 0x06
    {"RLCA", 0},                  // 0x07
    {"LD (%04X), SP", 2},         // 0x08
    {"ADD HL, BC", 0},            // 0x09
    {"LD A, (BC)", 0},            // 0x0A
    {"DEC BC", 0},                // 0x0B
    {"INC C", 0},                 // 0x0C
    {"DEC C", 0},                 // 0x0D
    {"LD C, %02X", 1},            // 0x0E
    {"RRCA", 0},                  // 0x0F
    {"STOP", 1},                  // 0x10
    {"LD DE, %04X", 2},           // 0x11
    {"LD (DE), A", 0},            // 0x12
    {"INC DE", 0},                // 0x13
    {"INC D", 0},                 // 0x14
    {"DEC D", 0},                 // 0x15
    {"LD D, %02X", 1},            // 0x16
    {"RLA", 0},                   // 0x17
    {"JR %02X", 1},               // 0x18
    {"ADD HL, DE", 0},            // 0x19
    {"LD A, (DE)", 0},            // 0x1A
    {"DEC DE", 0},                // 0x1B
    {"INC E", 0},                 // 0x1C
    {"DEC E", 0},                 // 0x1D
    {"LD E, %02X", 1},            // 0x1E
    {"RRA", 0},                   // 0x1F
    {"JR NZ, %02X", 1},           // 0x20
    {"LD HL, %04X", 2},           // 0x21
    {"LDI (HL), A", 0},           // 0x22
    {"INC HL", 0},                // 0x23
    {"INC H", 0},                 // 0x24
    {"DEC H", 0},                 // 0x25
    {"LD H,%02X", 1},             // 0x26
    {"DAA", 0},                   // 0x27
    {"JR Z, %02X", 1},            // 0x28
    {"ADD HL, HL", 0},            // 0x29
    {"LDI A, (HL)", 0},           // 0x2A
    {"DEC HL", 0},                // 0x2B
    {"INC L", 0},                 // 0x2C
    {"DEC L", 0},                 // 0x2D
    {"LD L, %02X", 1},            // 0x2E
    {"CPL", 0},                   // 0x2F
    {"JR NC, %02X", 1},           // 0x30
    {"LD SP, %04X", 2},           // 0x31
    {"LDD (HL), A", 0},           // 0x32
    {"INC SP", 0},                // 0x33
    {"INC (HL)", 0},              // 0x34
    {"DEC (HL)", 0},              // 0x35
    {"LD (HL), %02X", 1},         // 0x36
    {"SCF", 0},                   // 0x37
    {"JR C, %02X", 1},            // 0x38
    {"ADD HL, SP", 0},            // 0x39
    {"LDD A, (HL)", 0},           // 0x3A
    {"DEC SP", 0},                // 0x3B
    {"INC A", 0},                 // 0x3C
    {"DEC A", 0},                 // 0x3D
    {"LD A, %02X", 1},            // 0x3E
    {"CCF", 0},                   // 0x3F
    {"LD B, B", 0},               // 0x40
    {"LD B, C", 0},               // 0x41
    {"LD B, D", 0},               // 0x42
    {"LD B, E", 0},               // 0x43
    {"LD B, H", 0},               // 0x44
    {"LD B, L", 0},               // 0x45
    {"LD B, (HL)", 0},            // 0x46
    {"LD B, A", 0},               // 0x47
    {"LD C, B", 0},               // 0x48
    {"LD C, C", 0},               // 0x49
    {"LD C, D", 0},               // 0x4A
    {"LD C, E", 0},               // 0x4B
    {"LD C, H", 0},               // 0x4C
    {"LD C, L", 0},               // 0x4D
    {"LD C, (HL)", 0},            // 0x4E
    {"LD C, A", 0},               // 0x4F
    {"LD D, B", 0},               // 0x50
    {"LD D, C", 0},               // 0x51
    {"LD D, D", 0},               // 0x52
    {"LD D, E", 0},               // 0x53
    {"LD D, H", 0},               // 0x54
    {"LD D, L", 0},               // 0x55
    {"LD D, (HL)", 0},            // 0x56
    {"LD D, A", 0},               // 0x57
    {"LD E, B", 0},               // 0x58
    {"LD E, C", 0},               // 0x59
    {"LD E, D", 0},               // 0x5A
    {"LD E, E", 0},               // 0x5B
    {"LD E, H", 0},               // 0x5C
    {"LD E, L", 0},               // 0x5D
    {"LD E, (HL)", 0},            // 0x5E
    {"LD E, A", 0},               // 0x5F
    {"LD H, B", 0},               // 0x60
    {"LD H, C", 0},               // 0x61
    {"LD H, D", 0},               // 0x62
    {"LD H, E", 0},               // 0x63
    {"LD H, H", 0},               // 0x64
    {"LD H, L", 0},               // 0x65
    {"LD H, (HL)", 0},            // 0x66
    {"LD H, A", 0},               // 0x67
    {"LD L, B", 0},               // 0x68
    {"LD L, C", 0},               // 0x69
    {"LD L, D", 0},               // 0x6A
    {"LD L, E", 0},               // 0x6B
    {"LD L, H", 0},               // 0x6C
    {"LD L, L", 0},               // 0x6D
    {"LD L, (HL)", 0},            // 0x6E
    {"LD L, A", 0},               // 0x6F
    {"LD (HL), B", 0},            // 0x70
    {"LD (HL), C", 0},            // 0x71
    {"LD (HL), D", 0},            // 0x72
    {"LD (HL), E", 0},            // 0x73
    {"LD (HL), H", 0},            // 0x74
    {"LD (HL), L", 0},            // 0x75
    {"HALT", 0},                  // 0x76
    {"LD (HL), A", 0},            // 0x77
    {"LD A, B", 0},               // 0x78
    {"LD A, C", 0},               // 0x79
    {"LD A, D", 0},               // 0x7A
    {"LD A, E", 0},               // 0x7B
    {"LD A, H", 0},               // 0x7C
    {"LD A, L", 0},               // 0x7D
    {"LD A, (HL)", 0},            // 0x7E
    {"LD A, A", 0},               // 0x7F
    {"ADD A, B", 0},              // 0x80
    {"ADD A, C", 0},              // 0x81
    {"ADD A, D", 0},              // 0x82
    {"ADD A, E", 0},              // 0x83
    {"ADD A, H", 0},              // 0x84
    {"ADD A, L", 0},              // 0x85
    {"ADD A, (HL)", 0},           // 0x86
    {"ADD A, A", 0},              // 0x87
    {"ADC A, B", 0},              // 0x88
    {"ADC A, C", 0},              // 0x89
    {"ADC A, D", 0},              // 0x8A
    {"ADC A, E", 0},              // 0x8B
    {"ADC A, H", 0},              // 0x8C
    {"ADC A, L", 0},              // 0x8D
    {"ADC A, (HL)", 0},           // 0x8E
    {"ADC A", 0},                 // 0x8F
    {"SUB A, B", 0},              // 0x90
    {"SUB A, C", 0},              // 0x91
    {"SUB A, D", 0},              // 0x92
    {"SUB A, E", 0},              // 0x93
    {"SUB A, H", 0},              // 0x94
    {"SUB A, L", 0},              // 0x95
    {"SUB A, (HL)", 0},           // 0x96
    {"SUB A, A", 0},              // 0x97
    {"SBC A, B", 0},              // 0x98
    {"SBC A, C", 0},              // 0x99
    {"SBC A, D", 0},              // 0x9A
    {"SBC A, E", 0},              // 0x9B
    {"SBC A, H", 0},              // 0x9C
    {"SBC A, L", 0},              // 0x9D
    {"SBC A, (HL)", 0},           // 0x9E
    {"SBC A, A", 0},              // 0x9F
    {"AND B", 0},                 // 0xA0
    {"AND C", 0},                 // 0xA1
    {"AND D", 0},                 // 0xA2
    {"AND E", 0},                 // 0xA3
    {"AND H", 0},                 // 0xA4
    {"AND L", 0},                 // 0xA5
    {"AND (HL)", 0},              // 0xA6
    {"AND A", 0},                 // 0xA7
    {"XOR B", 0},                 // 0xA8
    {"XOR C", 0},                 // 0xA9
    {"XOR D", 0},                 // 0xAA
    {"XOR E", 0},                 // 0xAB
    {"XOR H", 0},                 // 0xAC
    {"XOR L", 0},                 // 0xAD
    {"XOR (HL)", 0},              // 0xAE
    {"XOR A", 0},                 // 0xAF
    {"OR B", 0},                  // 0xB0
    {"OR C", 0},                  // 0xB1
    {"OR D", 0},                  // 0xB2
    {"OR E", 0},                  // 0xB3
    {"OR H", 0},                  // 0xB4
    {"OR L", 0},                  // 0xB5
    {"OR (HL)", 0},               // 0xB6
    {"OR A", 0},                  // 0xB7
    {"CP B", 0},                  // 0xB8
    {"CP C", 0},                  // 0xB9
    {"CP D", 0},                  // 0xBA
    {"CP E", 0},                  // 0xBB
    {"CP H", 0},                  // 0xBC
    {"CP L", 0},                  // 0xBD
    {"CP (HL)", 0},               // 0xBE
    {"CP A", 0},                  // 0xBF
    {"RET NZ", 0},                // 0xC0
    {"POP BC", 0},                // 0xC1
    {"JP NZ, %04X", 2},           // 0xC2
    {"JP %04X", 2},               // 0xC3
    {"CALL NZ, %04X", 2},         // 0xC4
    {"PUSH BC", 0},               // 0xC5
    {"ADD A, %02X", 1},           // 0xC6
    {"RST 0x00", 0},              // 0xC7
    {"RET Z", 0},                 // 0xC8
    {"RET", 0},                   // 0xC9
    {"JP Z, %04X", 2},            // 0xCA
    {"CB %02X", 1},               // 0xCB
    {"CALL Z, %04X", 2},          // 0xCC
    {"CALL %04X", 2},             // 0xCD
    {"ADC %02X", 1},              // 0xCE
    {"RST 0x08", 0},              // 0xCF
    {"RET NC", 0},                // 0xD0
    {"POP DE", 0},                // 0xD1
    {"JP NC, %04X", 2},           // 0xD2
    {"UNDEFINED", 0},             // 0xD3
    {"CALL NC, %04X", 2},         // 0xD4
    {"PUSH DE", 0},               // 0xD5
    {"SUB A, %02X", 1},           // 0xD6
    {"RST 0x10", 0},              // 0xD7
    {"RET C", 0},                 // 0xD8
    {"RETI", 0},                  // 0xD9
    {"JP C, %04X", 2},            // 0xDA
    {"UNDEFINED", 0},             // 0xDB
    {"CALL C, %04X", 2},          // 0xDC
    {"UNDEFINED", 0},             // 0xDD
    {"SBC %02X", 1},              // 0xDE
    {"RST 0x18", 0},              // 0xDF
    {"LD (0xFF00 + %02X), A", 1}, // 0xE0
    {"POP HL", 0},                // 0xE1
    {"LD (0xFF00 + C), A", 0},    // 0xE2
    {"UNDEFINED", 0},             // 0xE3
    {"UNDEFINED", 0},             // 0xE4
    {"PUSH HL", 0},               // 0xE5
    {"AND %02X", 1},              // 0xE6
    {"RST 0x20", 0},              // 0xE7
    {"ADD SP,%02X", 1},           // 0xE8
    {"JP HL", 0},                 // 0xE9
    {"LD (%04X), A", 2},          // 0xEA
    {"UNDEFINED", 0},             // 0xEB
    {"UNDEFINED", 0},             // 0xEC
    {"UNDEFINED", 0},             // 0xED
    {"XOR %02X", 1},              // 0xEE
    {"RST 0x28", 0},              // 0xEF
    {"LD A, (0xFF00 + %02X)", 1}, // 0xF0
    {"POP AF", 0},                // 0xF1
    {"LD A, (0xFF00 + C)", 0},    // 0xF2
    {"DI", 0},                    // 0xF3
    {"UNDEFINED", 0},             // 0xF4
    {"PUSH AF", 0},               // 0xF5
    {"OR %02X", 1},               // 0xF6
    {"RST 0x30", 0},              // 0xF7
    {"LD HL, SP+%02X", 1},        // 0xF8
    {"LD SP, HL", 0},             // 0xF9
    {"LD A, (%04X)", 2},          // 0xFA
    {"EI", 0},                    // 0xFB
    {"UNDEFINED", 0},             // 0xFC
    {"UNDEFINED", 0},             // 0xFD
    {"CP %02X", 1},               // 0xFE
    {"RST 0x38", 0},              // 0xFF
};

const opcode_t extended_instructions[256] = {
    {"RLC B", 0},       // 0x00
    {"RLC C", 0},       // 0x01
    {"RLC D", 0},       // 0x02
    {"RLC E", 0},       // 0x03
    {"RLC H", 0},       // 0x04
    {"RLC L", 0},       // 0x05
    {"RLC (HL)", 0},    // 0x06
    {"RLC A", 0},       // 0x07
    {"RRC B", 0},       // 0x08
    {"RRC C", 0},       // 0x09
    {"RRC D", 0},       // 0x0A
    {"RRC E", 0},       // 0x0B
    {"RRC H", 0},       // 0x0C
    {"RRC L", 0},       // 0x0D
    {"RRC (HL)", 0},    // 0x0E
    {"RRC A", 0},       // 0x0F
    {"RL B", 0},        // 0x10
    {"RL C", 0},        // 0x11
    {"RL D", 0},        // 0x12
    {"RL E", 0},        // 0x13
    {"RL H", 0},        // 0x14
    {"RL L", 0},        // 0x15
    {"RL (HL)", 0},     // 0x16
    {"RL A", 0},        // 0x17
    {"RR B", 0},        // 0x18
    {"RR C", 0},        // 0x19
    {"RR D", 0},        // 0x1A
    {"RR E", 0},        // 0x1B
    {"RR H", 0},        // 0x1C
    {"RR L", 0},        // 0x1D
    {"RR (HL)", 0},     // 0x1E
    {"RR A", 0},        // 0x1F
    {"SLA B", 0},       // 0x20
    {"SLA C", 0},       // 0x21
    {"SLA D", 0},       // 0x22
    {"SLA E", 0},       // 0x23
    {"SLA H", 0},       // 0x24
    {"SLA L", 0},       // 0x25
    {"SLA (HL)", 0},    // 0x26
    {"SLA A", 0},       // 0x27
    {"SRA B", 0},       // 0x28
    {"SRA C", 0},       // 0x29
    {"SRA D", 0},       // 0x2A
    {"SRA E", 0},       // 0x2B
    {"SRA H", 0},       // 0x2C
    {"SRA L", 0},       // 0x2D
    {"SRA (HL)", 0},    // 0x2E
    {"SRA A", 0},       // 0x2F
    {"SWAP B", 0},      // 0x30
    {"SWAP C", 0},      // 0x31
    {"SWAP D", 0},      // 0x32
    {"SWAP E", 0},      // 0x33
    {"SWAP H", 0},      // 0x34
    {"SWAP L", 0},      // 0x35
    {"SWAP (HL)", 0},   // 0x36
    {"SWAP A", 0},      // 0x37
    {"SRL B", 0},       // 0x38
    {"SRL C", 0},       // 0x39
    {"SRL D", 0},       // 0x3A
    {"SRL E", 0},       // 0x3B
    {"SRL H", 0},       // 0x3C
    {"SRL L", 0},       // 0x3D
    {"SRL (HL)", 0},    // 0x3E
    {"SRL A", 0},       // 0x3F
    {"BIT 0, B", 0},    // 0x40
    {"BIT 0, C", 0},    // 0x41
    {"BIT 0, D", 0},    // 0x42
    {"BIT 0, E", 0},    // 0x43
    {"BIT 0, H", 0},    // 0x44
    {"BIT 0, L", 0},    // 0x45
    {"BIT 0, (HL)", 0}, // 0x46
    {"BIT 0, A", 0},    // 0x47
    {"BIT 1, B", 0},    // 0x48
    {"BIT 1, C", 0},    // 0x49
    {"BIT 1, D", 0},    // 0x4A
    {"BIT 1, E", 0},    // 0x4B
    {"BIT 1, H", 0},    // 0x4C
    {"BIT 1, L", 0},    // 0x4D
    {"BIT 1, (HL)", 0}, // 0x4E
    {"BIT 1, A", 0},    // 0x4F
    {"BIT 2, B", 0},    // 0x50
    {"BIT 2, C", 0},    // 0x51
    {"BIT 2, D", 0},    // 0x52
    {"BIT 2, E", 0},    // 0x53
    {"BIT 2, H", 0},    // 0x54
    {"BIT 2, L", 0},    // 0x55
    {"BIT 2, (HL)", 0}, // 0x56
    {"BIT 2, A", 0},    // 0x57
    {"BIT 3, B", 0},    // 0x58
    {"BIT 3, C", 0},    // 0x59
    {"BIT 3, D", 0},    // 0x5A
    {"BIT 3, E", 0},    // 0x5B
    {"BIT 3, H", 0},    // 0x5C
    {"BIT 3, L", 0},    // 0x5D
    {"BIT 3, (HL)", 0}, // 0x5E
    {"BIT 3, A", 0},    // 0x5F
    {"BIT 4, B", 0},    // 0x60
    {"BIT 4, C", 0},    // 0x61
    {"BIT 4, D", 0},    // 0x62
    {"BIT 4, E", 0},    // 0x63
    {"BIT 4, H", 0},    // 0x64
    {"BIT 4, L", 0},    // 0x65
    {"BIT 4, (HL)", 0}, // 0x66
    {"BIT 4, A", 0},    // 0x67
    {"BIT 5, B", 0},    // 0x68
    {"BIT 5, C", 0},    // 0x69
    {"BIT 5, D", 0},    // 0x6A
    {"BIT 5, E", 0},    // 0x6B
    {"BIT 5, H", 0},    // 0x6C
    {"BIT 5, L", 0},    // 0x6D
    {"BIT 5, (HL)", 0}, // 0x6E
    {"BIT 5, A", 0},    // 0x6F
    {"BIT 6, B", 0},    // 0x70
    {"BIT 6, C", 0},    // 0x71
    {"BIT 6, D", 0},    // 0x72
    {"BIT 6, E", 0},    // 0x73
    {"BIT 6, H", 0},    // 0x74
    {"BIT 6, L", 0},    // 0x75
    {"BIT 6, (HL)", 0}, // 0x76
    {"BIT 6, A", 0},    // 0x77
    {"BIT 7, B", 0},    // 0x78
    {"BIT 7, C", 0},    // 0x79
    {"BIT 7, D", 0},    // 0x7A
    {"BIT 7, E", 0},    // 0x7B
    {"BIT 7, H", 0},    // 0x7C
    {"BIT 7, L", 0},    // 0x7D
    {"BIT 7, (HL)", 0}, // 0x7E
    {"BIT 7, A", 0},    // 0x7F
    {"RES 0, B", 0},    // 0x80
    {"RES 0, C", 0},    // 0x81
    {"RES 0, D", 0},    // 0x82
    {"RES 0, E", 0},    // 0x83
    {"RES 0, H", 0},    // 0x84
    {"RES 0, L", 0},    // 0x85
    {"RES 0, (HL)", 0}, // 0x86
    {"RES 0, A", 0},    // 0x87
    {"RES 1, B", 0},    // 0x88
    {"RES 1, C", 0},    // 0x89
    {"RES 1, D", 0},    // 0x8A
    {"RES 1, E", 0},    // 0x8B
    {"RES 1, H", 0},    // 0x8C
    {"RES 1, L", 0},    // 0x8D
    {"RES 1, (HL)", 0}, // 0x8E
    {"RES 1, A", 0},    // 0x8F
    {"RES 2, B", 0},    // 0x90
    {"RES 2, C", 0},    // 0x91
    {"RES 2, D", 0},    // 0x92
    {"RES 2, E", 0},    // 0x93
    {"RES 2, H", 0},    // 0x94
    {"RES 2, L", 0},    // 0x95
    {"RES 2, (HL)", 0}, // 0x96
    {"RES 2, A", 0},    // 0x97
    {"RES 3, B", 0},    // 0x98
    {"RES 3, C", 0},    // 0x99
    {"RES 3, D", 0},    // 0x9A
    {"RES 3, E", 0},    // 0x9B
    {"RES 3, H", 0},    // 0x9C
    {"RES 3, L", 0},    // 0x9D
    {"RES 3, (HL)", 0}, // 0x9E
    {"RES 3, A", 0},    // 0x9F
    {"RES 4, B", 0},    // 0xA0
    {"RES 4, C", 0},    // 0xA1
    {"RES 4, D", 0},    // 0xA2
    {"RES 4, E", 0},    // 0xA3
    {"RES 4, H", 0},    // 0xA4
    {"RES 4, L", 0},    // 0xA5
    {"RES 4, (HL)", 0}, // 0xA6
    {"RES 4, A", 0},    // 0xA7
    {"RES 5, B", 0},    // 0xA8
    {"RES 5, C", 0},    // 0xA9
    {"RES 5, D", 0},    // 0xAA
    {"RES 5, E", 0},    // 0xAB
    {"RES 5, H", 0},    // 0xAC
    {"RES 5, L", 0},    // 0xAD
    {"RES 5, (HL)", 0}, // 0xAE
    {"RES 5, A", 0},    // 0xAF
    {"RES 6, B", 0},    // 0xB0
    {"RES 6, C", 0},    // 0xB1
    {"RES 6, D", 0},    // 0xB2
    {"RES 6, E", 0},    // 0xB3
    {"RES 6, H", 0},    // 0xB4
    {"RES 6, L", 0},    // 0xB5
    {"RES 6, (HL)", 0}, // 0xB6
    {"RES 6, A", 0},    // 0xB7
    {"RES 7, B", 0},    // 0xB8
    {"RES 7, C", 0},    // 0xB9
    {"RES 7, D", 0},    // 0xBA
    {"RES 7, E", 0},    // 0xBB
    {"RES 7, H", 0},    // 0xBC
    {"RES 7, L", 0},    // 0xBD
    {"RES 7, (HL)", 0}, // 0xBE
    {"RES 7, A", 0},    // 0xBF
    {"SET 0, B", 0},    // 0xC0
    {"SET 0, C", 0},    // 0xC1
    {"SET 0, D", 0},    // 0xC2
    {"SET 0, E", 0},    // 0xC3
    {"SET 0, H", 0},    // 0xC4
    {"SET 0, L", 0},    // 0xC5
    {"SET 0, (HL)", 0}, // 0xC6
    {"SET 0, A", 0},    // 0xC7
    {"SET 1, B", 0},    // 0xC8
    {"SET 1, C", 0},    // 0xC9
    {"SET 1, D", 0},    // 0xCA
    {"SET 1, E", 0},    // 0xCB
    {"SET 1, H", 0},    // 0xCC
    {"SET 1, L", 0},    // 0xCD
    {"SET 1, (HL)", 0}, // 0xCE
    {"SET 1, A", 0},    // 0xCF
    {"SET 2, B", 0},    // 0xD0
    {"SET 2, C", 0},    // 0xD1
    {"SET 2, D", 0},    // 0xD2
    {"SET 2, E", 0},    // 0xD3
    {"SET 2, H", 0},    // 0xD4
    {"SET 2, L", 0},    // 0xD5
    {"SET 2, (HL)", 0}, // 0xD6
    {"SET 2, A", 0},    // 0xD7
    {"SET 3, B", 0},    // 0xD8
    {"SET 3, C", 0},    // 0xD9
    {"SET 3, D", 0},    // 0xDA
    {"SET 3, E", 0},    // 0xDB
    {"SET 3, H", 0},    // 0xDC
    {"SET 3, L", 0},    // 0xDD
    {"SET 3, (HL)", 0}, // 0xDE
    {"SET 3, A", 0},    // 0xDF
    {"SET 4, B", 0},    // 0xE0
    {"SET 4, C", 0},    // 0xE1
    {"SET 4, D", 0},    // 0xE2
    {"SET 4, E", 0},    // 0xE3
    {"SET 4, H", 0},    // 0xE4
    {"SET 4, L", 0},    // 0xE5
    {"SET 4, (HL)", 0}, // 0xE6
    {"SET 4, A", 0},    // 0xE7
    {"SET 5, B", 0},    // 0xE8
    {"SET 5, C", 0},    // 0xE9
    {"SET 5, D", 0},    // 0xEA
    {"SET 5, E", 0},    // 0xEB
    {"SET 5, H", 0},    // 0xEC
    {"SET 5, L", 0},    // 0xED
    {"SET 5, (HL)", 0}, // 0xEE
    {"SET 5, A", 0},    // 0xEF
    {"SET 6, B", 0},    // 0xF0
    {"SET 6, C", 0},    // 0xF1
    {"SET 6, D", 0},    // 0xF2
    {"SET 6, E", 0},    // 0xF3
    {"SET 6, H", 0},    // 0xF4
    {"SET 6, L", 0},    // 0xF5
    {"SET 6, (HL)", 0}, // 0xF6
    {"SET 6, A", 0},    // 0xF7
    {"SET 7, B", 0},    // 0xF8
    {"SET 7, C", 0},    // 0xF9
    {"SET 7, D", 0},    // 0xFA
    {"SET 7, E", 0},    // 0xFB
    {"SET 7, H", 0},    // 0xFC
    {"SET 7, L", 0},    // 0xFD
    {"SET 7, (HL)", 0}, // 0xFE
    {"SET 7, A", 0},    // 0xFF
};

// takes 4 cycles
#define GET_OPERAND_8(...) \
    CLOCK(cpu->operand = mmu_read(gb, cpu->registers.pc); cpu->registers.pc++; __VA_ARGS__;);

// takes 8 cycles
#define GET_OPERAND_16(...)                                                           \
    {                                                                                 \
        CLOCK(cpu->operand = mmu_read(gb, cpu->registers.pc++));                     \
        CLOCK(cpu->operand |= mmu_read(gb, cpu->registers.pc++) << 8; __VA_ARGS__;); \
    }

// takes 12 cycles
#define PUSH(word, ...)                                                          \
    {                                                                            \
        CLOCK(mmu_write(gb, --cpu->registers.sp, (word) >> 8));                 \
        CLOCK(mmu_write(gb, --cpu->registers.sp, (word) & 0xFF); __VA_ARGS__;); \
    }

// takes 12 cycles
#define POP(reg_ptr, ...)                                                           \
    {                                                                               \
        CLOCK(*(reg_ptr) = mmu_read(gb, cpu->registers.sp++));                     \
        CLOCK(*(reg_ptr) |= mmu_read(gb, cpu->registers.sp++) << 8; __VA_ARGS__;); \
    }

// takes 12 or 24 cycles
#define CALL_CC(condition)                                    \
    {                                                         \
        GET_OPERAND_16();                                     \
        CLOCK(if ((condition)) END_OPCODE;);                  \
        PUSH(cpu->registers.pc);                              \
        CLOCK(cpu->registers.pc = cpu->operand; END_OPCODE;); \
    }

// takes 8 or 20 cycles
#define RET_CC(condition)                                                    \
    { /* can't use the POP macro here as the timings are a little tricky */  \
        CLOCK();                                                             \
        CLOCK(                                                               \
            if ((condition)) END_OPCODE;                                     \
            else cpu->registers.pc = mmu_read(gb, cpu->registers.sp++););   \
        CLOCK(cpu->registers.pc |= mmu_read(gb, cpu->registers.sp++) << 8); \
        CLOCK();                                                             \
        CLOCK(END_OPCODE);                                                   \
    }

static inline void and(gb_cpu_t *cpu, byte_t reg) {
    cpu->registers.a &= reg;
    cpu->registers.a ? RESET_FLAG(cpu, FLAG_Z) : SET_FLAG(cpu, FLAG_Z);
    RESET_FLAG(cpu, FLAG_N | FLAG_C);
    SET_FLAG(cpu, FLAG_H);
}

static inline void or(gb_cpu_t *cpu, byte_t reg) {
    cpu->registers.a |= reg;
    cpu->registers.a ? RESET_FLAG(cpu, FLAG_Z) : SET_FLAG(cpu, FLAG_Z);
    RESET_FLAG(cpu, FLAG_C | FLAG_H | FLAG_N);
}

static inline void xor(gb_cpu_t *cpu, byte_t reg) {
    cpu->registers.a ^= reg;
    cpu->registers.a ? RESET_FLAG(cpu, FLAG_Z) : SET_FLAG(cpu, FLAG_Z);
    RESET_FLAG(cpu, FLAG_C | FLAG_H | FLAG_N);
}

static inline void bit(gb_cpu_t *cpu, byte_t reg, byte_t pos) {
    CHECK_BIT(reg, pos) ? RESET_FLAG(cpu, FLAG_Z) : SET_FLAG(cpu, FLAG_Z);
    RESET_FLAG(cpu, FLAG_N);
    SET_FLAG(cpu, FLAG_H);
}

static inline void inc(gb_cpu_t *cpu, byte_t *reg) {
    // if lower nibble of reg is at max value, there will be a half carry after we increment.
    ((*reg) & 0x0F) == 0x0F ? SET_FLAG(cpu, FLAG_H) : RESET_FLAG(cpu, FLAG_H);
    (*reg)++;
    (*reg) ? RESET_FLAG(cpu, FLAG_Z) : SET_FLAG(cpu, FLAG_Z);
    RESET_FLAG(cpu, FLAG_N);
}

static inline void dec(gb_cpu_t *cpu, byte_t *reg) {
    // if lower nibble of reg is 0, there will be a half carry after we decrement (not sure if this is a sufficient condition)
    ((*reg) & 0x0F) == 0x00 ? SET_FLAG(cpu, FLAG_H) : RESET_FLAG(cpu, FLAG_H);
    (*reg)--;
    (*reg) ? RESET_FLAG(cpu, FLAG_Z) : SET_FLAG(cpu, FLAG_Z);
    SET_FLAG(cpu, FLAG_N);
}

static inline void rl(gb_cpu_t *cpu, byte_t *reg) {
    int carry = CHECK_FLAG(cpu, FLAG_C) ? 1 : 0;
    // set new carry to the value of leftmost bit
    ((*reg) & 0x80) ? SET_FLAG(cpu, FLAG_C) : RESET_FLAG(cpu, FLAG_C);
    // shift left
    (*reg) <<= 1;
    // set rightmost bit to the value of the old carry
    (*reg) += carry;

    (*reg) ? RESET_FLAG(cpu, FLAG_Z) : SET_FLAG(cpu, FLAG_Z);
    RESET_FLAG(cpu, FLAG_N | FLAG_H);
}

static inline void rlc(gb_cpu_t *cpu, byte_t *reg) {
    // set new carry to the value of leftmost bit
    ((*reg) & 0x80) ? SET_FLAG(cpu, FLAG_C) : RESET_FLAG(cpu, FLAG_C);
    // shift left
    (*reg) <<= 1;
    // set rightmost bit to the value of the carry (previous bit 7 value)
    (*reg) += CHECK_FLAG(cpu, FLAG_C) ? 1 : 0;

    (*reg) ? RESET_FLAG(cpu, FLAG_Z) : SET_FLAG(cpu, FLAG_Z);
    RESET_FLAG(cpu, FLAG_N | FLAG_H);
}

static inline void srl(gb_cpu_t *cpu, byte_t *reg) {
    // set carry to the value of rightmost bit
    ((*reg) & 0x01) ? SET_FLAG(cpu, FLAG_C) : RESET_FLAG(cpu, FLAG_C);
    // shift right
    (*reg) >>= 1;

    (*reg) ? RESET_FLAG(cpu, FLAG_Z) : SET_FLAG(cpu, FLAG_Z);
    RESET_FLAG(cpu, FLAG_N | FLAG_H);
}

static inline void sla(gb_cpu_t *cpu, byte_t *reg) {
    // set carry to the value of leftmost bit
    ((*reg) & 0x80) ? SET_FLAG(cpu, FLAG_C) : RESET_FLAG(cpu, FLAG_C);
    // shift left
    (*reg) <<= 1;

    (*reg) ? RESET_FLAG(cpu, FLAG_Z) : SET_FLAG(cpu, FLAG_Z);
    RESET_FLAG(cpu, FLAG_N | FLAG_H);
}

static inline void sra(gb_cpu_t *cpu, byte_t *reg) {
    // set carry to the value of rightmost bit
    int msb = (*reg) & 0x80;
    ((*reg) & 0x01) ? SET_FLAG(cpu, FLAG_C) : RESET_FLAG(cpu, FLAG_C);
    // shift right
    (*reg) >>= 1;
    (*reg) |= msb;

    (*reg) ? RESET_FLAG(cpu, FLAG_Z) : SET_FLAG(cpu, FLAG_Z);
    RESET_FLAG(cpu, FLAG_N | FLAG_H);
}

static inline void rr(gb_cpu_t *cpu, byte_t *reg) {
    int carry = CHECK_FLAG(cpu, FLAG_C) ? 1 : 0;
    // set new carry to the value of rightmost bit
    ((*reg) & 0x01) ? SET_FLAG(cpu, FLAG_C) : RESET_FLAG(cpu, FLAG_C);
    // shift right
    (*reg) >>= 1;
    // set leftmost bit to the value of the old carry
    carry ? SET_BIT((*reg), 7) : RESET_BIT((*reg), 7);

    (*reg) ? RESET_FLAG(cpu, FLAG_Z) : SET_FLAG(cpu, FLAG_Z);
    RESET_FLAG(cpu, FLAG_N | FLAG_H);
}

static inline void rrc(gb_cpu_t *cpu, byte_t *reg) {
    // set new carry to the value of rightmost bit
    ((*reg) & 0x01) ? SET_FLAG(cpu, FLAG_C) : RESET_FLAG(cpu, FLAG_C);
    // shift right
    (*reg) >>= 1;
    // set leftmost bit to the value of the carry (previous bit 0 value)
    CHECK_FLAG(cpu, FLAG_C) ? SET_BIT((*reg), 7) : RESET_BIT((*reg), 7);

    (*reg) ? RESET_FLAG(cpu, FLAG_Z) : SET_FLAG(cpu, FLAG_Z);
    RESET_FLAG(cpu, FLAG_N | FLAG_H);
}

static inline void swap(gb_cpu_t *cpu, byte_t *reg) {
    (*reg) = ((*reg) << 4) | ((*reg) >> 4);
    (*reg) ? RESET_FLAG(cpu, FLAG_Z) : SET_FLAG(cpu, FLAG_Z);
    RESET_FLAG(cpu, FLAG_C | FLAG_H | FLAG_N);
}

static inline void cp(gb_cpu_t *cpu, byte_t reg) {
    cpu->registers.a == reg ? SET_FLAG(cpu, FLAG_Z) : RESET_FLAG(cpu, FLAG_Z);
    reg > cpu->registers.a ? SET_FLAG(cpu, FLAG_C) : RESET_FLAG(cpu, FLAG_C);
    (reg & 0x0F) > (cpu->registers.a & 0x0F) ? SET_FLAG(cpu, FLAG_H) : RESET_FLAG(cpu, FLAG_H);
    SET_FLAG(cpu, FLAG_N);
}

static inline void add8(gb_cpu_t *cpu, byte_t val) {
    unsigned int result = cpu->registers.a + val;
    (result & 0xFF00) ? SET_FLAG(cpu, FLAG_C) : RESET_FLAG(cpu, FLAG_C);
    (((cpu->registers.a & 0x0F) + (val & 0x0F)) & 0x10) == 0x10 ? SET_FLAG(cpu, FLAG_H) : RESET_FLAG(cpu, FLAG_H);
    cpu->registers.a = result & 0xFF;
    cpu->registers.a ? RESET_FLAG(cpu, FLAG_Z) : SET_FLAG(cpu, FLAG_Z);
    RESET_FLAG(cpu, FLAG_N);
}

static inline void add16(gb_cpu_t *cpu, word_t val) {
    unsigned int result = cpu->registers.hl + val;
    (result & 0xFFFF0000) ? SET_FLAG(cpu, FLAG_C) : RESET_FLAG(cpu, FLAG_C);
    (((cpu->registers.hl & 0x0FFF) + (val & 0x0FFF)) & 0x1000) == 0x1000 ? SET_FLAG(cpu, FLAG_H) : RESET_FLAG(cpu, FLAG_H);
    cpu->registers.hl = result & 0xFFFF;
    RESET_FLAG(cpu, FLAG_N);
}

static inline void adc(gb_cpu_t *cpu, byte_t val) {
    byte_t c = CHECK_FLAG(cpu, FLAG_C) ? 1 : 0;
    unsigned int result = cpu->registers.a + val + c;
    (result & 0xFF00) ? SET_FLAG(cpu, FLAG_C) : RESET_FLAG(cpu, FLAG_C);
    (((cpu->registers.a & 0x0F) + (val & 0x0F) + c) & 0x10) == 0x10 ? SET_FLAG(cpu, FLAG_H) : RESET_FLAG(cpu, FLAG_H);
    cpu->registers.a = result & 0xFF;
    cpu->registers.a ? RESET_FLAG(cpu, FLAG_Z) : SET_FLAG(cpu, FLAG_Z);
    RESET_FLAG(cpu, FLAG_N);
}

static inline void sub8(gb_cpu_t *cpu, byte_t reg) {
    reg > cpu->registers.a ? SET_FLAG(cpu, FLAG_C) : RESET_FLAG(cpu, FLAG_C);
    (reg & 0x0F) > (cpu->registers.a & 0x0F) ? SET_FLAG(cpu, FLAG_H) : RESET_FLAG(cpu, FLAG_H);
    cpu->registers.a -= reg;
    cpu->registers.a ? RESET_FLAG(cpu, FLAG_Z) : SET_FLAG(cpu, FLAG_Z);
    SET_FLAG(cpu, FLAG_N);
}

static inline void sbc(gb_cpu_t *cpu, byte_t reg) {
    byte_t c = CHECK_FLAG(cpu, FLAG_C) ? 1 : 0;
    reg + c > cpu->registers.a ? SET_FLAG(cpu, FLAG_C) : RESET_FLAG(cpu, FLAG_C);
    ((reg & 0x0F) + c) > (cpu->registers.a & 0x0F) ? SET_FLAG(cpu, FLAG_H) : RESET_FLAG(cpu, FLAG_H);
    cpu->registers.a -= reg + c;
    cpu->registers.a ? RESET_FLAG(cpu, FLAG_Z) : SET_FLAG(cpu, FLAG_Z);
    SET_FLAG(cpu, FLAG_N);
}

static void exec_extended_opcode(gb_t *gb) {
    gb_cpu_t *cpu = gb->cpu;

    switch (cpu->opcode_state) {
    case 0x00: // RLC B (4 cycles)
        CLOCK(rlc(cpu, &cpu->registers.b); END_OPCODE;);
    case 0x01: // RLC C (4 cycles)
        CLOCK(rlc(cpu, &cpu->registers.c); END_OPCODE;);
    case 0x02: // RLC D (4 cycles)
        CLOCK(rlc(cpu, &cpu->registers.d); END_OPCODE;);
    case 0x03: // RLC E (4 cycles)
        CLOCK(rlc(cpu, &cpu->registers.e); END_OPCODE;);
    case 0x04: // RLC H (4 cycles)
        CLOCK(rlc(cpu, &cpu->registers.h); END_OPCODE;);
    case 0x05: // RLC L (4 cycles)
        CLOCK(rlc(cpu, &cpu->registers.l); END_OPCODE;);
    case 0x06: // RLC (HL) (12 cycles)
        CLOCK(
            cpu->accumulator = mmu_read(gb, cpu->registers.hl);
            rlc(cpu, (byte_t *) &cpu->accumulator);
        );
        CLOCK(mmu_write(gb, cpu->registers.hl, cpu->accumulator));
        CLOCK(END_OPCODE);
    case 0x07: // RLC A (4 cycles)
        CLOCK(rlc(cpu, &cpu->registers.a); END_OPCODE;);
    case 0x08: // RRC B (4 cycles)
        CLOCK(rrc(cpu, &cpu->registers.b); END_OPCODE);
    case 0x09: // RRC C (4 cycles)
        CLOCK(rrc(cpu, &cpu->registers.c); END_OPCODE);
    case 0x0A: // RRC D (4 cycles)
        CLOCK(rrc(cpu, &cpu->registers.d); END_OPCODE);
    case 0x0B: // RRC E (4 cycles)
        CLOCK(rrc(cpu, &cpu->registers.e); END_OPCODE);
    case 0x0C: // RRC H (4 cycles)
        CLOCK(rrc(cpu, &cpu->registers.h); END_OPCODE);
    case 0x0D: // RRC L (4 cycles)
        CLOCK(rrc(cpu, &cpu->registers.l); END_OPCODE);
    case 0x0E: // RRC (HL) (12 cycles)
        CLOCK(
            cpu->accumulator = mmu_read(gb, cpu->registers.hl);
            rrc(cpu, (byte_t *) &cpu->accumulator);
        );
        CLOCK(mmu_write(gb, cpu->registers.hl, cpu->accumulator));
        CLOCK(END_OPCODE);
    case 0x0F: // RRC A (4 cycles)
        CLOCK(rrc(cpu, &cpu->registers.a); END_OPCODE);
    case 0x10: // RL B (4 cycles)
        CLOCK(rl(cpu, &cpu->registers.b); END_OPCODE;);
    case 0x11: // RL C (4 cycles)
        CLOCK(rl(cpu, &cpu->registers.c); END_OPCODE;)
    case 0x12: // RL D (4 cycles)
        CLOCK(rl(cpu, &cpu->registers.d); END_OPCODE;);
    case 0x13: // RL E (4 cycles)
        CLOCK(rl(cpu, &cpu->registers.e); END_OPCODE;);
    case 0x14: // RL H (4 cycles)
        CLOCK(rl(cpu, &cpu->registers.h); END_OPCODE;);
    case 0x15: // RL L (4 cycles)
        CLOCK(rl(cpu, &cpu->registers.l); END_OPCODE;);
    case 0x16: // RL (HL) (12 cycles)
        CLOCK(
            cpu->accumulator = mmu_read(gb, cpu->registers.hl);
            rl(cpu, (byte_t *) &cpu->accumulator);
        );
        CLOCK(mmu_write(gb, cpu->registers.hl, cpu->accumulator));
        CLOCK(END_OPCODE);
    case 0x17: // RL A (4 cycles)
        CLOCK(rl(cpu, &cpu->registers.a); END_OPCODE;);
    case 0x18: // RR B (4 cycles)
        CLOCK(rr(cpu, &cpu->registers.b); END_OPCODE;);
    case 0x19: // RR C (4 cycles)
        CLOCK(rr(cpu, &cpu->registers.c); END_OPCODE;);
    case 0x1A: // RR D (4 cycles)
        CLOCK(rr(cpu, &cpu->registers.d); END_OPCODE;);
    case 0x1B: // RR E (4 cycles)
        CLOCK(rr(cpu, &cpu->registers.e); END_OPCODE;);
    case 0x1C: // RR H (4 cycles)
        CLOCK(rr(cpu, &cpu->registers.h); END_OPCODE;);
    case 0x1D: // RR L (4 cycles)
        CLOCK(rr(cpu, &cpu->registers.l); END_OPCODE;);
    case 0x1E: // RR (HL) (12 cycles)
        CLOCK(
            cpu->accumulator = mmu_read(gb, cpu->registers.hl);
            rr(cpu, (byte_t *) &cpu->accumulator);
        );
        CLOCK(mmu_write(gb, cpu->registers.hl, cpu->accumulator));
        CLOCK(END_OPCODE);
    case 0x1F: // RR A (4 cycles)
        CLOCK(rr(cpu, &cpu->registers.a); END_OPCODE;);
    case 0x20: // SLA B (4 cycles)
        CLOCK(sla(cpu, &cpu->registers.b); END_OPCODE;);
    case 0x21: // SLA C (4 cycles)
        CLOCK(sla(cpu, &cpu->registers.c); END_OPCODE;);
    case 0x22: // SLA D (4 cycles)
        CLOCK(sla(cpu, &cpu->registers.d); END_OPCODE;);
    case 0x23: // SLA E (4 cycles)
        CLOCK(sla(cpu, &cpu->registers.e); END_OPCODE;);
    case 0x24: // SLA H (4 cycles)
        CLOCK(sla(cpu, &cpu->registers.h); END_OPCODE;);
    case 0x25: // SLA L (4 cycles)
        CLOCK(sla(cpu, &cpu->registers.l); END_OPCODE;);
    case 0x26: // SLA (HL) (12 cycles)
        CLOCK(
            cpu->accumulator = mmu_read(gb, cpu->registers.hl);
            sla(cpu, (byte_t *) &cpu->accumulator);
        );
        CLOCK(mmu_write(gb, cpu->registers.hl, cpu->accumulator));
        CLOCK(END_OPCODE);
    case 0x27: // SLA A (4 cycles)
        CLOCK(sla(cpu, &cpu->registers.a); END_OPCODE;);
    case 0x28: // SRA B (4 cycles)
        CLOCK(sra(cpu, &cpu->registers.b); END_OPCODE;);
    case 0x29: // SRA C (4 cycles)
        CLOCK(sra(cpu, &cpu->registers.c); END_OPCODE;);
    case 0x2A: // SRA D (4 cycles)
        CLOCK(sra(cpu, &cpu->registers.d); END_OPCODE;);
    case 0x2B: // SRA E (4 cycles)
        CLOCK(sra(cpu, &cpu->registers.e); END_OPCODE;);
    case 0x2C: // SRA H (4 cycles)
        CLOCK(sra(cpu, &cpu->registers.h); END_OPCODE;);
    case 0x2D: // SRA L (4 cycles)
        CLOCK(sra(cpu, &cpu->registers.l); END_OPCODE;);
    case 0x2E: // SRA (HL) (12 cycles)
        CLOCK(
            cpu->accumulator = mmu_read(gb, cpu->registers.hl);
            sra(cpu, (byte_t *) &cpu->accumulator);
        );
        CLOCK(mmu_write(gb, cpu->registers.hl, cpu->accumulator));
        CLOCK(END_OPCODE);
    case 0x2F: // SRA A (4 cycles)
        CLOCK(sra(cpu, &cpu->registers.a); END_OPCODE;);
    case 0x30: // SWAP B (4 cycles)
        CLOCK(swap(cpu, &cpu->registers.b); END_OPCODE;);
    case 0x31: // SWAP C (4 cycles)
        CLOCK(swap(cpu, &cpu->registers.c); END_OPCODE;);
    case 0x32: // SWAP D (4 cycles)
        CLOCK(swap(cpu, &cpu->registers.d); END_OPCODE;);
    case 0x33: // SWAP E (4 cycles)
        CLOCK(swap(cpu, &cpu->registers.e); END_OPCODE;);
    case 0x34: // SWAP H (4 cycles)
        CLOCK(swap(cpu, &cpu->registers.h); END_OPCODE;);
    case 0x35: // SWAP L (4 cycles)
        CLOCK(swap(cpu, &cpu->registers.l); END_OPCODE;);
    case 0x36: // SWAP (HL) (12 cycles)
        CLOCK(
            cpu->accumulator = mmu_read(gb, cpu->registers.hl);
            swap(cpu, (byte_t *) &cpu->accumulator);
        );
        CLOCK(mmu_write(gb, cpu->registers.hl, cpu->accumulator));
        CLOCK(END_OPCODE);
    case 0x37: // SWAP A (4 cycles)
        CLOCK(swap(cpu, &cpu->registers.a); END_OPCODE;);
    case 0x38: // SRL B (4 cycles)
        CLOCK(srl(cpu, &cpu->registers.b); END_OPCODE;);
    case 0x39: // SRL C (4 cycles)
        CLOCK(srl(cpu, &cpu->registers.c); END_OPCODE;);
    case 0x3A: // SRL D (4 cycles)
        CLOCK(srl(cpu, &cpu->registers.d); END_OPCODE;);
    case 0x3B: // SRL E (4 cycles)
        CLOCK(srl(cpu, &cpu->registers.e); END_OPCODE;);
    case 0x3C: // SRL H (4 cycles)
        CLOCK(srl(cpu, &cpu->registers.h); END_OPCODE;);
    case 0x3D: // SRL L (4 cycles)
        CLOCK(srl(cpu, &cpu->registers.l); END_OPCODE;);
    case 0x3E: // SRL (HL) (12 cycles)
        CLOCK(
            cpu->accumulator = mmu_read(gb, cpu->registers.hl);
            srl(cpu, (byte_t *) &cpu->accumulator);
        );
        CLOCK(mmu_write(gb, cpu->registers.hl, cpu->accumulator));
        CLOCK(END_OPCODE);
    case 0x3F: // SRL A (4 cycles)
        CLOCK(srl(cpu, &cpu->registers.a); END_OPCODE;);
    case 0x40: // BIT 0, B (4 cycles)
        CLOCK(bit(cpu, cpu->registers.b, 0); END_OPCODE);
    case 0x41: // BIT 0, C (4 cycles)
        CLOCK(bit(cpu, cpu->registers.c, 0); END_OPCODE;);
    case 0x42: // BIT 0, D (4 cycles)
        CLOCK(bit(cpu, cpu->registers.d, 0); END_OPCODE);
    case 0x43: // BIT 0, E (4 cycles)
        CLOCK(bit(cpu, cpu->registers.e, 0); END_OPCODE);
    case 0x44: // BIT 0, H (4 cycles)
        CLOCK(bit(cpu, cpu->registers.h, 0); END_OPCODE);
    case 0x45: // BIT 0, L (4 cycles)
        CLOCK(bit(cpu, cpu->registers.l, 0); END_OPCODE);
    case 0x46: // BIT 0, (HL) (8 cycles)
        CLOCK(cpu->accumulator = mmu_read(gb, cpu->registers.hl));
        CLOCK(bit(cpu, cpu->accumulator, 0); END_OPCODE);
    case 0x47: // BIT 0, A (4 cycles)
        CLOCK(bit(cpu, cpu->registers.a, 0); END_OPCODE);
    case 0x48: // BIT 1, B (4 cycles)
        CLOCK(bit(cpu, cpu->registers.b, 1); END_OPCODE;);
    case 0x49: // BIT 1, C (4 cycles)
        CLOCK(bit(cpu, cpu->registers.c, 1); END_OPCODE;);
    case 0x4A: // BIT 1, D (4 cycles)
        CLOCK(bit(cpu, cpu->registers.d, 1); END_OPCODE;);
    case 0x4B: // BIT 1, E (4 cycles)
        CLOCK(bit(cpu, cpu->registers.e, 1); END_OPCODE;);
    case 0x4C: // BIT 1, H (4 cycles)
        CLOCK(bit(cpu, cpu->registers.h, 1); END_OPCODE;);
    case 0x4D: // BIT 1, L (4 cycles)
        CLOCK(bit(cpu, cpu->registers.l, 1); END_OPCODE;);
    case 0x4E: // BIT 1, (HL) (8 cycles)
        CLOCK(cpu->accumulator = mmu_read(gb, cpu->registers.hl));
        CLOCK(bit(cpu, cpu->accumulator, 1); END_OPCODE);
    case 0x4F: // BIT 1, A (4 cycles)
        CLOCK(bit(cpu, cpu->registers.a, 1); END_OPCODE;);
    case 0x50: // BIT 2, B (4 cycles)
        CLOCK(bit(cpu, cpu->registers.b, 2); END_OPCODE;);
    case 0x51: // BIT 2, C (4 cycles)
        CLOCK(bit(cpu, cpu->registers.c, 2); END_OPCODE;);
    case 0x52: // BIT 2, D (4 cycles)
        CLOCK(bit(cpu, cpu->registers.d, 2); END_OPCODE;);
    case 0x53: // BIT 2, E (4 cycles)
        CLOCK(bit(cpu, cpu->registers.e, 2); END_OPCODE;);
    case 0x54: // BIT 2, H (4 cycles)
        CLOCK(bit(cpu, cpu->registers.h, 2); END_OPCODE;);
    case 0x55: // BIT 2, L (4 cycles)
        CLOCK(bit(cpu, cpu->registers.l, 2); END_OPCODE;);
    case 0x56: // BIT 2, (HL) (8 cycles)
        CLOCK(cpu->accumulator = mmu_read(gb, cpu->registers.hl));
        CLOCK(bit(cpu, cpu->accumulator, 2); END_OPCODE);
    case 0x57: // BIT 2, A (4 cycles)
        CLOCK(bit(cpu, cpu->registers.a, 2); END_OPCODE;);
    case 0x58: // BIT 3, B (4 cycles)
        CLOCK(bit(cpu, cpu->registers.b, 3); END_OPCODE;);
    case 0x59: // BIT 3, C (4 cycles)
        CLOCK(bit(cpu, cpu->registers.c, 3); END_OPCODE;);
    case 0x5A: // BIT 3, D (4 cycles)
        CLOCK(bit(cpu, cpu->registers.d, 3); END_OPCODE;);
    case 0x5B: // BIT 3, E (4 cycles)
        CLOCK(bit(cpu, cpu->registers.e, 3); END_OPCODE;);
    case 0x5C: // BIT 3, H (4 cycles)
        CLOCK(bit(cpu, cpu->registers.h, 3); END_OPCODE;);
    case 0x5D: // BIT 3, L (4 cycles)
        CLOCK(bit(cpu, cpu->registers.l, 3); END_OPCODE;);
    case 0x5E: // BIT 3, (HL) (8 cycles)
        CLOCK(cpu->accumulator = mmu_read(gb, cpu->registers.hl));
        CLOCK(bit(cpu, cpu->accumulator, 3); END_OPCODE);
    case 0x5F: // BIT 3, A (4 cycles)
        CLOCK(bit(cpu, cpu->registers.a, 3); END_OPCODE;);
    case 0x60: // BIT 4, B (4 cycles)
        CLOCK(bit(cpu, cpu->registers.b, 4); END_OPCODE;);
    case 0x61: // BIT 4, C (4 cycles)
        CLOCK(bit(cpu, cpu->registers.c, 4); END_OPCODE;);
    case 0x62: // BIT 4, D (4 cycles)
        CLOCK(bit(cpu, cpu->registers.d, 4); END_OPCODE;);
    case 0x63: // BIT 4, E (4 cycles)
        CLOCK(bit(cpu, cpu->registers.e, 4); END_OPCODE;);
    case 0x64: // BIT 4, H (4 cycles)
        CLOCK(bit(cpu, cpu->registers.h, 4); END_OPCODE;);
    case 0x65: // BIT 4, L (4 cycles)
        CLOCK(bit(cpu, cpu->registers.l, 4); END_OPCODE;);
    case 0x66: // BIT 4, (HL) (8 cycles)
        CLOCK(cpu->accumulator = mmu_read(gb, cpu->registers.hl));
        CLOCK(bit(cpu, cpu->accumulator, 4); END_OPCODE);
    case 0x67: // BIT 4, A (4 cycles)
        CLOCK(bit(cpu, cpu->registers.a, 4); END_OPCODE;);
    case 0x68: // BIT 5, B (4 cycles)
        CLOCK(bit(cpu, cpu->registers.b, 5); END_OPCODE;);
    case 0x69: // BIT 5, C (4 cycles)
        CLOCK(bit(cpu, cpu->registers.c, 5); END_OPCODE;);
    case 0x6A: // BIT 5, D (4 cycles)
        CLOCK(bit(cpu, cpu->registers.d, 5); END_OPCODE;);
    case 0x6B: // BIT 5, E (4 cycles)
        CLOCK(bit(cpu, cpu->registers.e, 5); END_OPCODE;);
    case 0x6C: // BIT 5, H (4 cycles)
        CLOCK(bit(cpu, cpu->registers.h, 5); END_OPCODE;);
    case 0x6D: // BIT 5, L (4 cycles)
        CLOCK(bit(cpu, cpu->registers.l, 5); END_OPCODE;);
    case 0x6E: // BIT 5, (HL) (8 cycles)
        CLOCK(cpu->accumulator = mmu_read(gb, cpu->registers.hl));
        CLOCK(bit(cpu, cpu->accumulator, 5); END_OPCODE);
    case 0x6F: // BIT 5, A (4 cycles)
        CLOCK(bit(cpu, cpu->registers.a, 5); END_OPCODE;);
    case 0x70: // BIT 6, B (4 cycles)
        CLOCK(bit(cpu, cpu->registers.b, 6); END_OPCODE;);
    case 0x71: // BIT 6, C (4 cycles)
        CLOCK(bit(cpu, cpu->registers.c, 6); END_OPCODE;);
    case 0x72: // BIT 6, D (4 cycles)
        CLOCK(bit(cpu, cpu->registers.d, 6); END_OPCODE;);
    case 0x73: // BIT 6, E (4 cycles)
        CLOCK(bit(cpu, cpu->registers.e, 6); END_OPCODE;);
    case 0x74: // BIT 6, H (4 cycles)
        CLOCK(bit(cpu, cpu->registers.h, 6); END_OPCODE;);
    case 0x75: // BIT 6, L (4 cycles)
        CLOCK(bit(cpu, cpu->registers.l, 6); END_OPCODE;);
    case 0x76: // BIT 6, (HL) (8 cycles)
        CLOCK(cpu->accumulator = mmu_read(gb, cpu->registers.hl));
        CLOCK(bit(cpu, cpu->accumulator, 6); END_OPCODE);
    case 0x77: // BIT 6, A (4 cycles)
        CLOCK(bit(cpu, cpu->registers.a, 6); END_OPCODE;);
    case 0x78: // BIT 7, B (4 cycles)
        CLOCK(bit(cpu, cpu->registers.b, 7); END_OPCODE;);
    case 0x79: // BIT 7, C (4 cycles)
        CLOCK(bit(cpu, cpu->registers.c, 7); END_OPCODE;);
    case 0x7A: // BIT 7, D (4 cycles)
        CLOCK(bit(cpu, cpu->registers.d, 7); END_OPCODE;);
    case 0x7B: // BIT 7, E (4 cycles)
        CLOCK(bit(cpu, cpu->registers.e, 7); END_OPCODE;);
    case 0x7C: // BIT 7, H (4 cycles)
        CLOCK(bit(cpu, cpu->registers.h, 7); END_OPCODE;);
    case 0x7D: // BIT 7, L (4 cycles)
        CLOCK(bit(cpu, cpu->registers.l, 7); END_OPCODE;);
    case 0x7E: // BIT 7, (HL) (8 cycles)
        CLOCK(cpu->accumulator = mmu_read(gb, cpu->registers.hl));
        CLOCK(bit(cpu, cpu->accumulator, 7); END_OPCODE);
    case 0x7F: // BIT 7, A (4 cycles)
        CLOCK(bit(cpu, cpu->registers.a, 7); END_OPCODE;);
    case 0x80: // RES 0, B (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.b, 0); END_OPCODE;);
    case 0x81: // RES 0, C (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.c, 0); END_OPCODE;);
    case 0x82: // RES 0, D (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.d, 0); END_OPCODE;);
    case 0x83: // RES 0, E (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.e, 0); END_OPCODE;);
    case 0x84: // RES 0, H (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.h, 0); END_OPCODE;);
    case 0x85: // RES 0, L (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.l, 0); END_OPCODE;);
    case 0x86: // RES 0, (HL) (12 cycles)
        CLOCK(
            cpu->accumulator = mmu_read(gb, cpu->registers.hl);
            RESET_BIT(cpu->accumulator, 0);
        );
        CLOCK(mmu_write(gb, cpu->registers.hl, cpu->accumulator));
        CLOCK(END_OPCODE);
    case 0x87: // RES 0, A (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.a, 0); END_OPCODE;);
    case 0x88: // RES 1, B (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.b, 1); END_OPCODE;);
    case 0x89: // RES 1, C (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.c, 1); END_OPCODE;);
    case 0x8A: // RES 1, D (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.d, 1); END_OPCODE;);
    case 0x8B: // RES 1, E (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.e, 1); END_OPCODE;);
    case 0x8C: // RES 1, H (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.h, 1); END_OPCODE;);
    case 0x8D: // RES 1, L (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.l, 1); END_OPCODE;);
    case 0x8E: // RES 1, (HL) (12 cycles)
        CLOCK(
            cpu->accumulator = mmu_read(gb, cpu->registers.hl);
            RESET_BIT(cpu->accumulator, 1);
        );
        CLOCK(mmu_write(gb, cpu->registers.hl, cpu->accumulator));
        CLOCK(END_OPCODE);
    case 0x8F: // RES 1, A (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.a, 1); END_OPCODE;);
    case 0x90: // RES 2, B (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.b, 2); END_OPCODE;);
    case 0x91: // RES 2, C (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.c, 2); END_OPCODE;);
    case 0x92: // RES 2, D (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.d, 2); END_OPCODE;);
    case 0x93: // RES 2, E (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.e, 2); END_OPCODE;);
    case 0x94: // RES 2, H (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.h, 2); END_OPCODE;);
    case 0x95: // RES 2, L (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.l, 2); END_OPCODE;);
    case 0x96: // RES 2, (HL) (12 cycles)
        CLOCK(
            cpu->accumulator = mmu_read(gb, cpu->registers.hl);
            RESET_BIT(cpu->accumulator, 2);
        );
        CLOCK(mmu_write(gb, cpu->registers.hl, cpu->accumulator));
        CLOCK(END_OPCODE);
    case 0x97: // RES 2, A (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.a, 2); END_OPCODE;);
    case 0x98: // RES 3, B (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.b, 3); END_OPCODE;);
    case 0x99: // RES 3, C (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.c, 3); END_OPCODE;);
    case 0x9A: // RES 3, D (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.d, 3); END_OPCODE;);
    case 0x9B: // RES 3, E (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.e, 3); END_OPCODE;);
    case 0x9C: // RES 3, H (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.h, 3); END_OPCODE;);
    case 0x9D: // RES 3, L (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.l, 3); END_OPCODE;);
    case 0x9E: // RES 3, (HL) (12 cycles)
        CLOCK(
            cpu->accumulator = mmu_read(gb, cpu->registers.hl);
            RESET_BIT(cpu->accumulator, 3);
        );
        CLOCK(mmu_write(gb, cpu->registers.hl, cpu->accumulator));
        CLOCK(END_OPCODE);
    case 0x9F: // RES 3, A (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.a, 3); END_OPCODE;);
    case 0xA0: // RES 4, B (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.b, 4); END_OPCODE;);
    case 0xA1: // RES 4, C (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.c, 4); END_OPCODE;);
    case 0xA2: // RES 4, D (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.d, 4); END_OPCODE;);
    case 0xA3: // RES 4, E (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.e, 4); END_OPCODE;);
    case 0xA4: // RES 4, H (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.h, 4); END_OPCODE;);
    case 0xA5: // RES 4, L (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.l, 4); END_OPCODE;);
    case 0xA6: // RES 4, (HL) (12 cycles)
        CLOCK(
            cpu->accumulator = mmu_read(gb, cpu->registers.hl);
            RESET_BIT(cpu->accumulator, 4);
        );
        CLOCK(mmu_write(gb, cpu->registers.hl, cpu->accumulator));
        CLOCK(END_OPCODE);
    case 0xA7: // RES 4, A (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.a, 4); END_OPCODE;);
    case 0xA8: // RES 5, B (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.b, 5); END_OPCODE;);
    case 0xA9: // RES 5, C (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.c, 5); END_OPCODE;);
    case 0xAA: // RES 5, D (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.d, 5); END_OPCODE;);
    case 0xAB: // RES 5, E (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.e, 5); END_OPCODE;);
    case 0xAC: // RES 5, H (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.h, 5); END_OPCODE;);
    case 0xAD: // RES 5, L (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.l, 5); END_OPCODE;);
    case 0xAE: // RES 5, (HL) (12 cycles)
        CLOCK(
            cpu->accumulator = mmu_read(gb, cpu->registers.hl);
            RESET_BIT(cpu->accumulator, 5);
        );
        CLOCK(mmu_write(gb, cpu->registers.hl, cpu->accumulator));
        CLOCK(END_OPCODE);
    case 0xAF: // RES 5, A (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.a, 5); END_OPCODE;);
    case 0xB0: // RES 6, B (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.b, 6); END_OPCODE;);
    case 0xB1: // RES 6, C (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.c, 6); END_OPCODE;);
    case 0xB2: // RES 6, D (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.d, 6); END_OPCODE;);
    case 0xB3: // RES 6, E (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.e, 6); END_OPCODE;);
    case 0xB4: // RES 6, H (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.h, 6); END_OPCODE;);
    case 0xB5: // RES 6, L (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.l, 6); END_OPCODE;);
    case 0xB6: // RES 6, (HL) (12 cycles)
        CLOCK(
            cpu->accumulator = mmu_read(gb, cpu->registers.hl);
            RESET_BIT(cpu->accumulator, 6);
        );
        CLOCK(mmu_write(gb, cpu->registers.hl, cpu->accumulator));
        CLOCK(END_OPCODE);
    case 0xB7: // RES 6, A (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.a, 6); END_OPCODE;);
    case 0xB8: // RES 7, B (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.b, 7); END_OPCODE;);
    case 0xB9: // RES 7, C (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.c, 7); END_OPCODE;);
    case 0xBA: // RES 7, D (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.d, 7); END_OPCODE;);
    case 0xBB: // RES 7, E (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.e, 7); END_OPCODE;);
    case 0xBC: // RES 7, H (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.h, 7); END_OPCODE;);
    case 0xBD: // RES 7, L (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.l, 7); END_OPCODE;);
    case 0xBE: // RES 7, (HL) (12 cycles)
        CLOCK(
            cpu->accumulator = mmu_read(gb, cpu->registers.hl);
            RESET_BIT(cpu->accumulator, 7);
        );
        CLOCK(mmu_write(gb, cpu->registers.hl, cpu->accumulator));
        CLOCK(END_OPCODE);
    case 0xBF: // RES 7, A (4 cycles)
        CLOCK(RESET_BIT(cpu->registers.a, 7); END_OPCODE;);
    case 0xC0: // SET 0, B (4 cycles)
        CLOCK(SET_BIT(cpu->registers.b, 0); END_OPCODE;);
    case 0xC1: // SET 0, C (4 cycles)
        CLOCK(SET_BIT(cpu->registers.c, 0); END_OPCODE;);
    case 0xC2: // SET 0, D (4 cycles)
        CLOCK(SET_BIT(cpu->registers.d, 0); END_OPCODE;);
    case 0xC3: // SET 0, E (4 cycles)
        CLOCK(SET_BIT(cpu->registers.e, 0); END_OPCODE;);
    case 0xC4: // SET 0, H (4 cycles)
        CLOCK(SET_BIT(cpu->registers.h, 0); END_OPCODE;);
    case 0xC5: // SET 0, L (4 cycles)
        CLOCK(SET_BIT(cpu->registers.l, 0); END_OPCODE;);
    case 0xC6: // SET 0, (HL) (12 cycles)
        CLOCK(
            cpu->accumulator = mmu_read(gb, cpu->registers.hl);
            SET_BIT(cpu->accumulator, 0);
        );
        CLOCK(mmu_write(gb, cpu->registers.hl, cpu->accumulator));
        CLOCK(END_OPCODE);
    case 0xC7: // SET 0, A (4 cycles)
        CLOCK(SET_BIT(cpu->registers.a, 0); END_OPCODE;);
    case 0xC8: // SET 1, B (4 cycles)
        CLOCK(SET_BIT(cpu->registers.b, 1); END_OPCODE;);
    case 0xC9: // SET 1, C (4 cycles)
        CLOCK(SET_BIT(cpu->registers.c, 1); END_OPCODE;);
    case 0xCA: // SET 1, D (4 cycles)
        CLOCK(SET_BIT(cpu->registers.d, 1); END_OPCODE;);
    case 0xCB: // SET 1, E (4 cycles)
        CLOCK(SET_BIT(cpu->registers.e, 1); END_OPCODE;);
    case 0xCC: // SET 1, H (4 cycles)
        CLOCK(SET_BIT(cpu->registers.h, 1); END_OPCODE;);
    case 0xCD: // SET 1, L (4 cycles)
        CLOCK(SET_BIT(cpu->registers.l, 1); END_OPCODE;);
    case 0xCE: // SET 1, (HL) (12 cycles)
        CLOCK(
            cpu->accumulator = mmu_read(gb, cpu->registers.hl);
            SET_BIT(cpu->accumulator, 1);
        );
        CLOCK(mmu_write(gb, cpu->registers.hl, cpu->accumulator));
        CLOCK(END_OPCODE);
    case 0xCF: // SET 1, A (4 cycles)
        CLOCK(SET_BIT(cpu->registers.a, 1); END_OPCODE;);
    case 0xD0: // SET 2, B (4 cycles)
        CLOCK(SET_BIT(cpu->registers.b, 2); END_OPCODE;);
    case 0xD1: // SET 2, C (4 cycles)
        CLOCK(SET_BIT(cpu->registers.c, 2); END_OPCODE;);
    case 0xD2: // SET 2, D (4 cycles)
        CLOCK(SET_BIT(cpu->registers.d, 2); END_OPCODE;);
    case 0xD3: // SET 2, E (4 cycles)
        CLOCK(SET_BIT(cpu->registers.e, 2); END_OPCODE;);
    case 0xD4: // SET 2, H (4 cycles)
        CLOCK(SET_BIT(cpu->registers.h, 2); END_OPCODE;);
    case 0xD5: // SET 2, L (4 cycles)
        CLOCK(SET_BIT(cpu->registers.l, 2); END_OPCODE;);
    case 0xD6: // SET 2, (HL) (12 cycles)
        CLOCK(
            cpu->accumulator = mmu_read(gb, cpu->registers.hl);
            SET_BIT(cpu->accumulator, 2);
        );
        CLOCK(mmu_write(gb, cpu->registers.hl, cpu->accumulator));
        CLOCK(END_OPCODE);
    case 0xD7: // SET 2, A (4 cycles)
        CLOCK(SET_BIT(cpu->registers.a, 2); END_OPCODE;);
    case 0xD8: // SET 3, B (4 cycles)
        CLOCK(SET_BIT(cpu->registers.b, 3); END_OPCODE;);
    case 0xD9: // SET 3, C (4 cycles)
        CLOCK(SET_BIT(cpu->registers.c, 3); END_OPCODE;);
    case 0xDA: // SET 3, D (4 cycles)
        CLOCK(SET_BIT(cpu->registers.d, 3); END_OPCODE;);
    case 0xDB: // SET 3, E (4 cycles)
        CLOCK(SET_BIT(cpu->registers.e, 3); END_OPCODE;);
    case 0xDC: // SET 3, H (4 cycles)
        CLOCK(SET_BIT(cpu->registers.h, 3); END_OPCODE;);
    case 0xDD: // SET 3, L (4 cycles)
        CLOCK(SET_BIT(cpu->registers.l, 3); END_OPCODE;);
    case 0xDE: // SET 3, (HL) (12 cycles)
        CLOCK(
            cpu->accumulator = mmu_read(gb, cpu->registers.hl);
            SET_BIT(cpu->accumulator, 3);
        );
        CLOCK(mmu_write(gb, cpu->registers.hl, cpu->accumulator));
        CLOCK(END_OPCODE);
    case 0xDF: // SET 3, A (4 cycles)
        CLOCK(SET_BIT(cpu->registers.a, 3); END_OPCODE;);
    case 0xE0: // SET 4, B (4 cycles)
        CLOCK(SET_BIT(cpu->registers.b, 4); END_OPCODE;);
    case 0xE1: // SET 4, C (4 cycles)
        CLOCK(SET_BIT(cpu->registers.c, 4); END_OPCODE;);
    case 0xE2: // SET 4, D (4 cycles)
        CLOCK(SET_BIT(cpu->registers.d, 4); END_OPCODE;);
    case 0xE3: // SET 4, E (4 cycles)
        CLOCK(SET_BIT(cpu->registers.e, 4); END_OPCODE;);
    case 0xE4: // SET 4, H (4 cycles)
        CLOCK(SET_BIT(cpu->registers.h, 4); END_OPCODE;);
    case 0xE5: // SET 4, L (4 cycles)
        CLOCK(SET_BIT(cpu->registers.l, 4); END_OPCODE;);
    case 0xE6: // SET 4, (HL) (12 cycles)
        CLOCK(
            cpu->accumulator = mmu_read(gb, cpu->registers.hl);
            SET_BIT(cpu->accumulator, 4);
        );
        CLOCK(mmu_write(gb, cpu->registers.hl, cpu->accumulator));
        CLOCK(END_OPCODE);
    case 0xE7: // SET 4, A (4 cycles)
        CLOCK(SET_BIT(cpu->registers.a, 4); END_OPCODE;);
    case 0xE8: // SET 5, B (4 cycles)
        CLOCK(SET_BIT(cpu->registers.b, 5); END_OPCODE;);
    case 0xE9: // SET 5, C (4 cycles)
        CLOCK(SET_BIT(cpu->registers.c, 5); END_OPCODE;);
    case 0xEA: // SET 5, D (4 cycles)
        CLOCK(SET_BIT(cpu->registers.d, 5); END_OPCODE;);
    case 0xEB: // SET 5, E (4 cycles)
        CLOCK(SET_BIT(cpu->registers.e, 5); END_OPCODE;);
    case 0xEC: // SET 5, H (4 cycles)
        CLOCK(SET_BIT(cpu->registers.h, 5); END_OPCODE;);
    case 0xED: // SET 5, L (4 cycles)
        CLOCK(SET_BIT(cpu->registers.l, 5); END_OPCODE;);
    case 0xEE: // SET 5, (HL) (12 cycles)
        CLOCK(
            cpu->accumulator = mmu_read(gb, cpu->registers.hl);
            SET_BIT(cpu->accumulator, 5);
        );
        CLOCK(mmu_write(gb, cpu->registers.hl, cpu->accumulator));
        CLOCK(END_OPCODE);
    case 0xEF: // SET 5, A (4 cycles)
        CLOCK(SET_BIT(cpu->registers.a, 5); END_OPCODE;);
    case 0xF0: // SET 6, B (4 cycles)
        CLOCK(SET_BIT(cpu->registers.b, 6); END_OPCODE;);
    case 0xF1: // SET 6, C (4 cycles)
        CLOCK(SET_BIT(cpu->registers.c, 6); END_OPCODE;);
    case 0xF2: // SET 6, D (4 cycles)
        CLOCK(SET_BIT(cpu->registers.d, 6); END_OPCODE;);
    case 0xF3: // SET 6, E (4 cycles)
        CLOCK(SET_BIT(cpu->registers.e, 6); END_OPCODE;);
    case 0xF4: // SET 6, H (4 cycles)
        CLOCK(SET_BIT(cpu->registers.h, 6); END_OPCODE;);
    case 0xF5: // SET 6, L (4 cycles)
        CLOCK(SET_BIT(cpu->registers.l, 6); END_OPCODE;);
    case 0xF6: // SET 6, (HL) (12 cycles)
        CLOCK(
            cpu->accumulator = mmu_read(gb, cpu->registers.hl);
            SET_BIT(cpu->accumulator, 6);
        );
        CLOCK(mmu_write(gb, cpu->registers.hl, cpu->accumulator));
        CLOCK(END_OPCODE);
    case 0xF7: // SET 6, A (4 cycles)
        CLOCK(SET_BIT(cpu->registers.a, 6); END_OPCODE;);
    case 0xF8: // SET 7, B (4 cycles)
        CLOCK(SET_BIT(cpu->registers.b, 7); END_OPCODE;);
    case 0xF9: // SET 7, C (4 cycles)
        CLOCK(SET_BIT(cpu->registers.c, 7); END_OPCODE;);
    case 0xFA: // SET 7, D (4 cycles)
        CLOCK(SET_BIT(cpu->registers.d, 7); END_OPCODE;);
    case 0xFB: // SET 7, E (4 cycles)
        CLOCK(SET_BIT(cpu->registers.e, 7); END_OPCODE;);
    case 0xFC: // SET 7, H (4 cycles)
        CLOCK(SET_BIT(cpu->registers.h, 7); END_OPCODE;);
    case 0xFD: // SET 7, L (4 cycles)
        CLOCK(SET_BIT(cpu->registers.l, 7); END_OPCODE;);
    case 0xFE: // SET 7, (HL) (12 cycles)
        CLOCK(
            cpu->accumulator = mmu_read(gb, cpu->registers.hl);
            SET_BIT(cpu->accumulator, 7)
        );
        CLOCK(mmu_write(gb, cpu->registers.hl, cpu->accumulator));
        CLOCK(END_OPCODE);
    case 0xFF: // SET 7, A (4 cycles)
        CLOCK(SET_BIT(cpu->registers.a, 7); END_OPCODE;);
    }
}

static void exec_opcode(gb_t *gb) {
    gb_cpu_t *cpu = gb->cpu;

    switch (cpu->opcode_state) {
    case 0x00: // NOP (4 cycles)
        CLOCK(END_OPCODE);
    case 0x01: // LD BC, nn (12 cycles)
        GET_OPERAND_16();
        CLOCK(cpu->registers.bc = cpu->operand; END_OPCODE;);
    case 0x02: // LD (BC),A (8 cycles)
        CLOCK(mmu_write(gb, cpu->registers.bc, cpu->registers.a));
        CLOCK(END_OPCODE);
    case 0x03: // INC BC (8 cycles)
        CLOCK(cpu->registers.bc++);
        CLOCK(END_OPCODE);
    case 0x04: // INC B (4 cycles)
        CLOCK(inc(cpu, &cpu->registers.b); END_OPCODE;);
    case 0x05: // DEC B (4 cycles)
        CLOCK(dec(cpu, &cpu->registers.b); END_OPCODE;);
    case 0x06: // LD B, n (8 cycles)
        GET_OPERAND_8();
        CLOCK(cpu->registers.b = cpu->operand; END_OPCODE;);
    case 0x07: // RLCA (4 cycles)
        CLOCK(
            rlc(cpu, &cpu->registers.a);
            RESET_FLAG(cpu, FLAG_Z);
            END_OPCODE;
        );
    case 0x08: // LD (nn), SP (20 cycles)
        GET_OPERAND_16();
        CLOCK(mmu_write(gb, cpu->operand, cpu->registers.sp & 0xFF));
        CLOCK(mmu_write(gb, cpu->operand + 1, cpu->registers.sp >> 8));
        CLOCK(END_OPCODE);
    case 0x09: // ADD HL, BC (8 cycles)
        CLOCK(add16(cpu, cpu->registers.bc));
        CLOCK(END_OPCODE);
    case 0x0A: // LD A,(BC) (8 cycles)
        CLOCK(cpu->accumulator = mmu_read(gb, cpu->registers.bc));
        CLOCK(cpu->registers.a = cpu->accumulator; END_OPCODE;);
    case 0x0B: // DEC BC (8 cycles)
        CLOCK(cpu->registers.bc--);
        CLOCK(END_OPCODE);
    case 0x0C: // INC C (4 cycles)
        CLOCK(inc(cpu, &cpu->registers.c); END_OPCODE;);
    case 0x0D: // DEC C (4 cycles)
        CLOCK(dec(cpu, &cpu->registers.c); END_OPCODE;);
    case 0x0E: // LD C, n (8 cycles)
        GET_OPERAND_8();
        CLOCK(cpu->registers.c = cpu->operand; END_OPCODE;);
    case 0x0F: // RRCA (4 cycles)
        CLOCK(
            rrc(cpu, &cpu->registers.a);
            RESET_FLAG(cpu, FLAG_Z);
            END_OPCODE;
        );
    case 0x10: // STOP (4 cycles)
        // TODO Halts until button press.
        CLOCK(
            // reset timer to 0
            gb->timer->div_timer = 0;
            if (PREPARE_SPEED_SWITCH(gb)) {
                // TODO this should also stop the cpu for 2050 steps (8200 cycles)
                // https://gbdev.io/pandocs/CGB_Registers.html?highlight=key1#ff4d--key1-cgb-mode-only-prepare-speed-switch
                if (IS_DOUBLE_SPEED(gb))
                    DISABLE_DOUBLE_SPEED(gb);
                else
                    ENABLE_DOUBLE_SPEED(gb);
            }
            eprintf("STOP instruction not fully implemented\n");
            END_OPCODE;
        );
    case 0x11: // LD DE, nn (12 cycles)
        GET_OPERAND_16();
        CLOCK(cpu->registers.de = cpu->operand; END_OPCODE;);
    case 0x12: // LD (DE),A (8 cycles)
        CLOCK(mmu_write(gb, cpu->registers.de, cpu->registers.a));
        CLOCK(END_OPCODE);
    case 0x13: // INC DE (8 cycles)
        CLOCK(cpu->accumulator = cpu->registers.de);
        CLOCK(cpu->registers.de = cpu->accumulator + 1; END_OPCODE;);
    case 0x14: // INC D (4 cycles)
        CLOCK(inc(cpu, &cpu->registers.d); END_OPCODE;);
    case 0x15: // DEC D (4 cycles)
        CLOCK(dec(cpu, &cpu->registers.d); END_OPCODE;);
    case 0x16: // LD D, n (8 cycles)
        GET_OPERAND_8();
        CLOCK(cpu->registers.d = cpu->operand; END_OPCODE;);
    case 0x17: // RLA (4 cycles)
        CLOCK(
            rl(cpu, &cpu->registers.a);
            RESET_FLAG(cpu, FLAG_Z);
            END_OPCODE;
        );
    case 0x18: // JR n (12 cycles)
        GET_OPERAND_8();
        CLOCK(cpu->registers.pc += (s_byte_t) cpu->operand);
        CLOCK(END_OPCODE);
    case 0x19: // ADD HL, DE (8 cycles)
        CLOCK(add16(cpu, cpu->registers.de));
        CLOCK(END_OPCODE);
    case 0x1A: // LD A,(DE) (8 cycles)
        CLOCK(cpu->accumulator = mmu_read(gb, cpu->registers.de));
        CLOCK(cpu->registers.a = cpu->accumulator; END_OPCODE;);
    case 0x1B: // DEC DE (8 cycles)
        CLOCK(cpu->registers.de--);
        CLOCK(END_OPCODE);        
    case 0x1C: // INC E (4 cycles)
        CLOCK(inc(cpu, &cpu->registers.e); END_OPCODE;);
    case 0x1D: // DEC E (4 cycles)
        CLOCK(dec(cpu, &cpu->registers.e); END_OPCODE;);
    case 0x1E: // LD E, n (8 cycles)
        GET_OPERAND_8();
        CLOCK(cpu->registers.e = cpu->operand; END_OPCODE;);
    case 0x1F: // RRA (4 cycles)
        CLOCK(
            rr(cpu, &cpu->registers.a);
            RESET_FLAG(cpu, FLAG_Z);
            END_OPCODE;
        );
    case 0x20: // JR NZ, n (8 or 12 cycles)
        GET_OPERAND_8();
        CLOCK(
            if (CHECK_FLAG(cpu, FLAG_Z))
                END_OPCODE;
        );
        CLOCK(cpu->registers.pc += (s_byte_t) cpu->operand; END_OPCODE;);
    case 0x21: // LD HL, nn (12 cycles)
        GET_OPERAND_16();
        CLOCK(cpu->registers.hl = cpu->operand; END_OPCODE;);
    case 0x22: // LDI (HL), A (8 cycles)
        CLOCK(mmu_write(gb, cpu->registers.hl, cpu->registers.a));
        CLOCK(cpu->registers.hl++; END_OPCODE;);
    case 0x23: // INC HL (8 cycles)
        CLOCK(cpu->registers.hl = cpu->registers.hl + 1);
        CLOCK(END_OPCODE);
    case 0x24: // INC H (4 cycles)
        CLOCK(inc(cpu, &cpu->registers.h); END_OPCODE;);
    case 0x25: // DEC H (4 cycles)
        CLOCK(dec(cpu, &cpu->registers.h); END_OPCODE;);
    case 0x26: // LD H, n (8 cycles)
        GET_OPERAND_8();
        CLOCK(cpu->registers.h = cpu->operand; END_OPCODE;);
    case 0x27: // DAA (4 cycles)
        CLOCK(
            if (!CHECK_FLAG(cpu, FLAG_N)) {
                if (CHECK_FLAG(cpu, FLAG_C) || cpu->registers.a > 0x99) {
                    cpu->registers.a += 0x60;
                    SET_FLAG(cpu, FLAG_C);
                }
                if (CHECK_FLAG(cpu, FLAG_H) || (cpu->registers.a & 0x0f) > 0x09)
                    cpu->registers.a += 0x06;
            } else {
                if (CHECK_FLAG(cpu, FLAG_C)) {
                    cpu->registers.a -= 0x60;
                    SET_FLAG(cpu, FLAG_C);
                }
                if (CHECK_FLAG(cpu, FLAG_H))
                    cpu->registers.a -= 0x06;
            }
            cpu->registers.a ? RESET_FLAG(cpu, FLAG_Z) : SET_FLAG(cpu, FLAG_Z);
            RESET_FLAG(cpu, FLAG_H);
            END_OPCODE;
        );
    case 0x28: // JR Z, n (8 or 12 cycles)
        GET_OPERAND_8();
        CLOCK(
            if (!CHECK_FLAG(cpu, FLAG_Z))
                END_OPCODE;
        );
        CLOCK(cpu->registers.pc += (s_byte_t) cpu->operand; END_OPCODE;);
    case 0x29: // ADD HL, HL (8 cycles)
        CLOCK(add16(cpu, cpu->registers.hl));
        CLOCK(END_OPCODE);
    case 0x2A: // LDI A, (HL) (8 cycles)
        CLOCK(cpu->accumulator = mmu_read(gb, cpu->registers.hl));
        CLOCK(
            cpu->registers.a = cpu->accumulator;
            cpu->registers.hl++;
            END_OPCODE;
        );
    case 0x2B: // DEC HL (8 cycles)
        CLOCK(cpu->registers.hl--);
        CLOCK(END_OPCODE);
    case 0x2C: // INC L (4 cycles)
        CLOCK(inc(cpu, &cpu->registers.l); END_OPCODE;);
    case 0x2D: // DEC L (4 cycles)
        CLOCK(dec(cpu, &cpu->registers.l); END_OPCODE;);
    case 0x2E: // LD L, n (8 cycles)
        GET_OPERAND_8();
        CLOCK(cpu->registers.l = cpu->operand; END_OPCODE;)
    case 0x2F: // CPL (4 cycles)
        CLOCK(
            cpu->registers.a = ~cpu->registers.a;
            SET_FLAG(cpu, FLAG_N | FLAG_H);
            END_OPCODE;
        );
    case 0x30: // JR NC, n (8 or 12 cycles)
        GET_OPERAND_8();
        CLOCK(
            if (CHECK_FLAG(cpu, FLAG_C))
                END_OPCODE;
        );
        CLOCK(cpu->registers.pc += (s_byte_t) cpu->operand; END_OPCODE;);
    case 0x31: // LD SP,nn (12 cycles)
        GET_OPERAND_16();
        CLOCK(cpu->registers.sp = cpu->operand; END_OPCODE;);
    case 0x32: // LDD (HL),A (8 cycles)
        CLOCK(mmu_write(gb, cpu->registers.hl, cpu->registers.a));
        CLOCK(cpu->registers.hl--; END_OPCODE;);
    case 0x33: // INC SP (8 cycles)
        CLOCK(cpu->registers.sp++);
        CLOCK(END_OPCODE);
    case 0x34: // INC (HL) (12 cycles)
        CLOCK(
            cpu->accumulator = mmu_read(gb, cpu->registers.hl);
            inc(cpu, (byte_t *) &cpu->accumulator);
        );
        CLOCK(mmu_write(gb, cpu->registers.hl, cpu->accumulator));
        CLOCK(END_OPCODE);
    case 0x35: // DEC (HL) (12 cycles)
        CLOCK(
            cpu->accumulator = mmu_read(gb, cpu->registers.hl);
            dec(cpu, (byte_t *) &cpu->accumulator);
        );
        CLOCK(mmu_write(gb, cpu->registers.hl, cpu->accumulator));
        CLOCK(END_OPCODE);
    case 0x36: // LD (HL),n (12 cycles)
        GET_OPERAND_8();
        CLOCK(mmu_write(gb, cpu->registers.hl, cpu->operand));
        CLOCK(END_OPCODE);
    case 0x37: // SCF (4 cycles)
        CLOCK(
            RESET_FLAG(cpu, FLAG_N | FLAG_H);
            SET_FLAG(cpu, FLAG_C);
            END_OPCODE;
        );
    case 0x38: // JR C, n (8 or 12 cycles)
        GET_OPERAND_8();
        CLOCK(
            if (!CHECK_FLAG(cpu, FLAG_C))
                END_OPCODE;
        );
        CLOCK(cpu->registers.pc += (s_byte_t) cpu->operand; END_OPCODE;);
    case 0x39: // ADD HL, SP (8 cycles)
        CLOCK(add16(cpu, cpu->registers.sp));
        CLOCK(END_OPCODE);
    case 0x3A: // LDD A, (HL) (8 cycles)
        CLOCK(cpu->registers.a = mmu_read(gb, cpu->registers.hl));
        CLOCK(cpu->registers.hl--; END_OPCODE;);
    case 0x3B: // DEC SP (8 cycles)
        CLOCK(cpu->registers.sp--);
        CLOCK(END_OPCODE);
    case 0x3C: // INC A (4 cycles)
        CLOCK(inc(cpu, &cpu->registers.a); END_OPCODE;);
    case 0x3D: // DEC A (4 cycles)
        CLOCK(dec(cpu, &cpu->registers.a); END_OPCODE;)
    case 0x3E: // LD A,n (8 cycles)
        GET_OPERAND_8();
        CLOCK(cpu->registers.a = cpu->operand; END_OPCODE;);
    case 0x3F: // CCF (4 cycles)
        CLOCK(
            CHECK_FLAG(cpu, FLAG_C) ? RESET_FLAG(cpu, FLAG_C) : SET_FLAG(cpu, FLAG_C);
            RESET_FLAG(cpu, FLAG_N | FLAG_H);
            END_OPCODE;
        );
    case 0x40: // LD B,B (4 cycles)
        CLOCK(/*cpu->registers.b = cpu->registers.b;*/ END_OPCODE;);
    case 0x41: // LD B,C (4 cycles)
        CLOCK(cpu->registers.b = cpu->registers.c; END_OPCODE;);
    case 0x42: // LD B,D (4 cycles)
        CLOCK(cpu->registers.b = cpu->registers.d; END_OPCODE;);
    case 0x43: // LD B,E (4 cycles)
        CLOCK(cpu->registers.b = cpu->registers.e; END_OPCODE;);
    case 0x44: // LD B,H (4 cycles)
        CLOCK(cpu->registers.b = cpu->registers.h; END_OPCODE;);
    case 0x45: // LD B,L (4 cycles)
        CLOCK(cpu->registers.b = cpu->registers.l; END_OPCODE;);
    case 0x46: // LD B,(HL) (8 cycles)
        CLOCK(cpu->accumulator = mmu_read(gb, cpu->registers.hl));
        CLOCK(cpu->registers.b = cpu->accumulator; END_OPCODE);
    case 0x47: // LD B,A (4 cycles)
        CLOCK(cpu->registers.b = cpu->registers.a; END_OPCODE;);
    case 0x48: // LD C,B (4 cycles)
        CLOCK(cpu->registers.c = cpu->registers.b; END_OPCODE;);
    case 0x49: // LD C,C (4 cycles)
        CLOCK(/*cpu->registers.c = cpu->registers.c;*/ END_OPCODE;);
    case 0x4A: // LD C,D (4 cycles)
        CLOCK(cpu->registers.c = cpu->registers.d; END_OPCODE;);
    case 0x4B: // LD C,E (4 cycles)
        CLOCK(cpu->registers.c = cpu->registers.e; END_OPCODE;);
    case 0x4C: // LD C,H (4 cycles)
        CLOCK(cpu->registers.c = cpu->registers.h; END_OPCODE;);
    case 0x4D: // LD C,L (4 cycles)
        CLOCK(cpu->registers.c = cpu->registers.l; END_OPCODE;);
    case 0x4E: // LD C,(HL) (8 cycles)
        CLOCK(cpu->accumulator = mmu_read(gb, cpu->registers.hl));
        CLOCK(cpu->registers.c = cpu->accumulator; END_OPCODE;);
    case 0x4F: // LD C,A (4 cycles)
        CLOCK(cpu->registers.c = cpu->registers.a; END_OPCODE;);
    case 0x50: // LD D,B (4 cycles)
        CLOCK(cpu->registers.d = cpu->registers.b; END_OPCODE;);
    case 0x51: // LD D,C (4 cycles)
        CLOCK(cpu->registers.d = cpu->registers.c; END_OPCODE;);
    case 0x52: // LD D,D (4 cycles)
        CLOCK(/*cpu->registers.d = cpu->registers.d;*/ END_OPCODE;);
    case 0x53: // LD D,E (4 cycles)
        CLOCK(cpu->registers.d = cpu->registers.e; END_OPCODE;);
    case 0x54: // LD D,H (4 cycles)
        CLOCK(cpu->registers.d = cpu->registers.h; END_OPCODE;);
    case 0x55: // LD D,L (4 cycles)
        CLOCK(cpu->registers.d = cpu->registers.l; END_OPCODE;);
    case 0x56: // LD D,(HL) (8 cycles)
        CLOCK(cpu->accumulator = mmu_read(gb, cpu->registers.hl));
        CLOCK(cpu->registers.d = cpu->accumulator; END_OPCODE;);
    case 0x57: // LD D,A (4 cycles)
        CLOCK(cpu->registers.d = cpu->registers.a; END_OPCODE;);
    case 0x58: // LD E,B (4 cycles)
        CLOCK(cpu->registers.e = cpu->registers.b; END_OPCODE;);
    case 0x59: // LD E,C (4 cycles)
        CLOCK(cpu->registers.e = cpu->registers.c; END_OPCODE;);
    case 0x5A: // LD E,D (4 cycles)
        CLOCK(cpu->registers.e = cpu->registers.d; END_OPCODE;);
    case 0x5B: // LD E,E (4 cycles)
        CLOCK(/*cpu->registers.e = cpu->registers.e;*/ END_OPCODE;);
    case 0x5C: // LD E,H (4 cycles)
        CLOCK(cpu->registers.e = cpu->registers.h; END_OPCODE;);
    case 0x5D: // LD E,L (4 cycles)
        CLOCK(cpu->registers.e = cpu->registers.l; END_OPCODE;);
    case 0x5E: // LD E,(HL) (8 cycles)
        CLOCK(cpu->accumulator = mmu_read(gb, cpu->registers.hl));
        CLOCK(cpu->registers.e = cpu->accumulator; END_OPCODE;);
    case 0x5F: // LD E,A (4 cycles)
        CLOCK(cpu->registers.e = cpu->registers.a; END_OPCODE;);
    case 0x60: // LD H,B (4 cycles)
        CLOCK(cpu->registers.h = cpu->registers.b; END_OPCODE;);
    case 0x61: // LD H,C (4 cycles)
        CLOCK(cpu->registers.h = cpu->registers.c; END_OPCODE;);
    case 0x62: // LD H,D (4 cycles)
        CLOCK(cpu->registers.h = cpu->registers.d; END_OPCODE;);
    case 0x63: // LD H,E (4 cycles)
        CLOCK(cpu->registers.h = cpu->registers.e; END_OPCODE;);
    case 0x64: // LD H,H (4 cycles)
        CLOCK(/*cpu->registers.h = cpu->registers.h;*/ END_OPCODE;);
    case 0x65: // LD H,L (4 cycles)
        CLOCK(cpu->registers.h = cpu->registers.l; END_OPCODE;);
    case 0x66: // LD H,(HL) (8 cycles)
        CLOCK(cpu->accumulator = mmu_read(gb, cpu->registers.hl));
        CLOCK(cpu->registers.h = cpu->accumulator; END_OPCODE;);
    case 0x67: // LD H,A (4 cycles)
        CLOCK(cpu->registers.h = cpu->registers.a; END_OPCODE;);
    case 0x68: // LD L,B (4 cycles)
        CLOCK(cpu->registers.l = cpu->registers.b; END_OPCODE;);
    case 0x69: // LD L,C (4 cycles)
        CLOCK(cpu->registers.l = cpu->registers.c; END_OPCODE;);
    case 0x6A: // LD L,D (4 cycles)
        CLOCK(cpu->registers.l = cpu->registers.d; END_OPCODE;);
    case 0x6B: // LD L,E (4 cycles)
        CLOCK(cpu->registers.l = cpu->registers.e; END_OPCODE;);
    case 0x6C: // LD L,H (4 cycles)
        CLOCK(cpu->registers.l = cpu->registers.h; END_OPCODE;);
    case 0x6D: // LD L,L (4 cycles)
        CLOCK(/*cpu->registers.l = cpu->registers.l;*/ END_OPCODE;);
    case 0x6E: // LD L,(HL) (8 cycles)
        CLOCK(cpu->accumulator = mmu_read(gb, cpu->registers.hl));
        CLOCK(cpu->registers.l = cpu->accumulator; END_OPCODE;);
    case 0x6F: // LD L,A (4 cycles)
        CLOCK(cpu->registers.l = cpu->registers.a; END_OPCODE;);
    case 0x70: // LD (HL),B (8 cycles)
        CLOCK(mmu_write(gb, cpu->registers.hl, cpu->registers.b));
        CLOCK(END_OPCODE);
    case 0x71: // LD (HL),C (8 cycles)
        CLOCK(mmu_write(gb, cpu->registers.hl, cpu->registers.c));
        CLOCK(END_OPCODE);
    case 0x72: // LD (HL),D (8 cycles)
        CLOCK(mmu_write(gb, cpu->registers.hl, cpu->registers.d));
        CLOCK(END_OPCODE);
    case 0x73: // LD (HL),E (8 cycles)
        CLOCK(mmu_write(gb, cpu->registers.hl, cpu->registers.e));
        CLOCK(END_OPCODE);
    case 0x74: // LD (HL),H (8 cycles)
        CLOCK(mmu_write(gb, cpu->registers.hl, cpu->registers.h));
        CLOCK(END_OPCODE);
    case 0x75: // LD (HL),L (8 cycles)
        CLOCK(mmu_write(gb, cpu->registers.hl, cpu->registers.l));
        CLOCK(END_OPCODE);
    case 0x76: // HALT (4 cycles)
        CLOCK(
            if (cpu->ime != IME_ENABLED && IS_INTERRUPT_PENDING(gb))
                cpu->halt_bug = 1;
            else
                cpu->halt = 1;
            END_OPCODE;
        );
    case 0x77: // LD (HL),A (8 cycles)
        CLOCK(mmu_write(gb, cpu->registers.hl, cpu->registers.a));
        CLOCK(END_OPCODE);
    case 0x78: // LD A,B (4 cycles)
        CLOCK(cpu->registers.a = cpu->registers.b; END_OPCODE;);
    case 0x79: // LD A,C (4 cycles)
        CLOCK(cpu->registers.a = cpu->registers.c; END_OPCODE;);
    case 0x7A: // LD A,D (4 cycles)
        CLOCK(cpu->registers.a = cpu->registers.d; END_OPCODE;);
    case 0x7B: // LD A,E (4 cycles)
        CLOCK(cpu->registers.a = cpu->registers.e; END_OPCODE;);
    case 0x7C: // LD A,H (4 cycles)
        CLOCK(cpu->registers.a = cpu->registers.h; END_OPCODE;);
    case 0x7D: // LD A,L (4 cycles)
        CLOCK(cpu->registers.a = cpu->registers.l; END_OPCODE;);
    case 0x7E: // LD A,(HL) (8 cycles)
        CLOCK(cpu->accumulator = mmu_read(gb, cpu->registers.hl));
        CLOCK(cpu->registers.a = cpu->accumulator; END_OPCODE;);
    case 0x7F: // LD A,A (4 cycles)
        CLOCK(/*cpu->registers.a = cpu->registers.a;*/ END_OPCODE;);
    case 0x80: // ADD A, B (4 cycles)
        CLOCK(add8(cpu, cpu->registers.b); END_OPCODE;);
    case 0x81: // ADD A, C (4 cycles)
        CLOCK(add8(cpu, cpu->registers.c); END_OPCODE;);
    case 0x82: // ADD A, D (4 cycles)
        CLOCK(add8(cpu, cpu->registers.d); END_OPCODE;);
    case 0x83: // ADD A, E (4 cycles)
        CLOCK(add8(cpu, cpu->registers.e); END_OPCODE;);
    case 0x84: // ADD A, H (4 cycles)
        CLOCK(add8(cpu, cpu->registers.h); END_OPCODE;);
    case 0x85: // ADD A, L (4 cycles)
        CLOCK(add8(cpu, cpu->registers.l); END_OPCODE;);
    case 0x86: // ADD A, (HL) (8 cycles)
        CLOCK(cpu->accumulator = mmu_read(gb, cpu->registers.hl));
        CLOCK(add8(cpu, cpu->accumulator); END_OPCODE;);
    case 0x87: // ADD A, A (4 cycles)
        CLOCK(add8(cpu, cpu->registers.a); END_OPCODE;);
    case 0x88: // ADC A, B (4 cycles)
        CLOCK(adc(cpu, cpu->registers.b); END_OPCODE;);
    case 0x89: // ADC A, C (4 cycles)
        CLOCK(adc(cpu, cpu->registers.c); END_OPCODE;);
    case 0x8A: // ADC A, D (4 cycles)
        CLOCK(adc(cpu, cpu->registers.d); END_OPCODE;);
    case 0x8B: // ADC A, E (4 cycles)
        CLOCK(adc(cpu, cpu->registers.e); END_OPCODE;);
    case 0x8C: // ADC A, H (4 cycles)
        CLOCK(adc(cpu, cpu->registers.h); END_OPCODE;);
    case 0x8D: // ADC A, L (4 cycles)
        CLOCK(adc(cpu, cpu->registers.l); END_OPCODE;);
    case 0x8E: // ADC A, (HL) (8 cycles)
        CLOCK(cpu->accumulator = mmu_read(gb, cpu->registers.hl));
        CLOCK(adc(cpu, cpu->accumulator); END_OPCODE;);
    case 0x8F: // ADC A, A (4 cycles)
        CLOCK(adc(cpu, cpu->registers.a); END_OPCODE;);
    case 0x90: // SUB A, B (4 cycles)
        CLOCK(sub8(cpu, cpu->registers.b); END_OPCODE;);
    case 0x91: // SUB A, C (4 cycles)
        CLOCK(sub8(cpu, cpu->registers.c); END_OPCODE;);
    case 0x92: // SUB A, D (4 cycles)
        CLOCK(sub8(cpu, cpu->registers.d); END_OPCODE;);
    case 0x93: // SUB A, E (4 cycles)
        CLOCK(sub8(cpu, cpu->registers.e); END_OPCODE;);
    case 0x94: // SUB A, H (4 cycles)
        CLOCK(sub8(cpu, cpu->registers.h); END_OPCODE;);
    case 0x95: // SUB A, L (4 cycles)
        CLOCK(sub8(cpu, cpu->registers.l); END_OPCODE;);
    case 0x96: // SUB A, (HL) (8 cycles)
        CLOCK(cpu->accumulator = mmu_read(gb, cpu->registers.hl));
        CLOCK(sub8(cpu, cpu->accumulator); END_OPCODE);
    case 0x97: // SUB A, A (4 cycles)
        CLOCK(sub8(cpu, cpu->registers.a); END_OPCODE;);
    case 0x98: // SBC A, B (4 cycles)
        CLOCK(sbc(cpu, cpu->registers.b); END_OPCODE;);
    case 0x99: // SBC A, C (4 cycles)
        CLOCK(sbc(cpu, cpu->registers.c); END_OPCODE;);
    case 0x9A: // SBC A, D (4 cycles)
        CLOCK(sbc(cpu, cpu->registers.d); END_OPCODE;);
    case 0x9B: // SBC A, E (4 cycles)
        CLOCK(sbc(cpu, cpu->registers.e); END_OPCODE;);
    case 0x9C: // SBC A, H (4 cycles)
        CLOCK(sbc(cpu, cpu->registers.h); END_OPCODE;);
    case 0x9D: // SBC A, L (4 cycles)
        CLOCK(sbc(cpu, cpu->registers.l); END_OPCODE;);
    case 0x9E: // SBC A, (HL) (8 cycles)
        CLOCK(cpu->accumulator = mmu_read(gb, cpu->registers.hl));
        CLOCK(sbc(cpu, cpu->accumulator); END_OPCODE;);
    case 0x9F: // SBC A, A (4 cycles)
        CLOCK(sbc(cpu, cpu->registers.a); END_OPCODE;);
    case 0xA0: // AND B (4 cycles)
        CLOCK(and(cpu, cpu->registers.b); END_OPCODE;);
    case 0xA1: // AND C (4 cycles)
        CLOCK(and(cpu, cpu->registers.c); END_OPCODE);
    case 0xA2: // AND D (4 cycles)
        CLOCK(and(cpu, cpu->registers.d); END_OPCODE;);
    case 0xA3: // AND E (4 cycles)
        CLOCK(and(cpu, cpu->registers.e); END_OPCODE;);
    case 0xA4: // AND H (4 cycles)
        CLOCK(and(cpu, cpu->registers.h); END_OPCODE;);
    case 0xA5: // AND L (4 cycles)
        CLOCK(and(cpu, cpu->registers.l); END_OPCODE;);
    case 0xA6: // AND (HL) (8 cycles)
        CLOCK(cpu->accumulator = mmu_read(gb, cpu->registers.hl));
        CLOCK(and(cpu, cpu->accumulator); END_OPCODE;);
    case 0xA7: // AND A (4 cycles)
        CLOCK(and(cpu, cpu->registers.a); END_OPCODE;);
    case 0xA8: // XOR B (4 cycles)
        CLOCK(xor(cpu, cpu->registers.b); END_OPCODE;);
    case 0xA9: // XOR C (4 cycles)
        CLOCK(xor(cpu, cpu->registers.c); END_OPCODE;);
    case 0xAA: // XOR D (4 cycles)
        CLOCK(xor(cpu, cpu->registers.d); END_OPCODE;);
    case 0xAB: // XOR E (4 cycles)
        CLOCK(xor(cpu, cpu->registers.e); END_OPCODE;);
    case 0xAC: // XOR H (4 cycles)
        CLOCK(xor(cpu, cpu->registers.h); END_OPCODE;);
    case 0xAD: // XOR L (4 cycles)
        CLOCK(xor(cpu, cpu->registers.l); END_OPCODE;);
    case 0xAE: // XOR (HL) (8 cycles)
        CLOCK(cpu->accumulator = mmu_read(gb, cpu->registers.hl));
        CLOCK(xor(cpu, cpu->accumulator); END_OPCODE;);
    case 0xAF: // XOR A (4 cycles)
        CLOCK(xor(cpu, cpu->registers.a); END_OPCODE;);
    case 0xB0: // OR B (4 cycles)
        CLOCK(or(cpu, cpu->registers.b); END_OPCODE;);
    case 0xB1: // OR C (4 cycles)
        CLOCK(or(cpu, cpu->registers.c); END_OPCODE;);
    case 0xB2: // OR D (4 cycles)
        CLOCK(or(cpu, cpu->registers.d); END_OPCODE;);
    case 0xB3: // OR E (4 cycles)
        CLOCK(or(cpu, cpu->registers.e); END_OPCODE;);
    case 0xB4: // OR H (4 cycles)
        CLOCK(or(cpu, cpu->registers.h); END_OPCODE;);
    case 0xB5: // OR L (4 cycles)
        CLOCK(or(cpu, cpu->registers.l); END_OPCODE;);
    case 0xB6: // OR (HL) (8 cycles)
        CLOCK(cpu->accumulator = mmu_read(gb, cpu->registers.hl));
        CLOCK(or(cpu, cpu->accumulator); END_OPCODE;);
    case 0xB7: // OR A (4 cycles)
        CLOCK(or(cpu, cpu->registers.a); END_OPCODE;);
    case 0xB8: // CP B (4 cycles)
        CLOCK(cp(cpu, cpu->registers.b); END_OPCODE;);
    case 0xB9: // CP C (4 cycles)
        CLOCK(cp(cpu, cpu->registers.c); END_OPCODE;);
    case 0xBA: // CP D (4 cycles)
        CLOCK(cp(cpu, cpu->registers.d); END_OPCODE;);
    case 0xBB: // CP E (4 cycles)
        CLOCK(cp(cpu, cpu->registers.e); END_OPCODE;);
    case 0xBC: // CP H (4 cycles)
        CLOCK(cp(cpu, cpu->registers.h); END_OPCODE;);
    case 0xBD: // CP L (4 cycles)
        CLOCK(cp(cpu, cpu->registers.l); END_OPCODE;);
    case 0xBE: // CP (HL) (8 cycles)
        CLOCK(cpu->accumulator = mmu_read(gb, cpu->registers.hl));
        CLOCK(cp(cpu, cpu->accumulator); END_OPCODE;);
    case 0xBF: // CP A (4 cycles)
        CLOCK(cp(cpu, cpu->registers.a); END_OPCODE;);
    case 0xC0: // RET NZ (8 or 20 cycles)
        RET_CC(CHECK_FLAG(cpu, FLAG_Z));
    case 0xC1: // POP BC (12 cycles)
        POP(&cpu->registers.bc);
        CLOCK(END_OPCODE);
    case 0xC2: // JP NZ, nn (12 or 16 cycles)
        GET_OPERAND_16();
        CLOCK(
            if (CHECK_FLAG(cpu, FLAG_Z))
                END_OPCODE;
        );
        CLOCK(cpu->registers.pc = cpu->operand; END_OPCODE;);
    case 0xC3: // JP nn (16 cycles)
        GET_OPERAND_16();
        CLOCK(cpu->registers.pc = cpu->operand);
        CLOCK(END_OPCODE);
    case 0xC4: // CALL NZ, nn (12 or 24 cycles)
        CALL_CC(CHECK_FLAG(cpu, FLAG_Z));
    case 0xC5: // PUSH BC (16 cycles)
        CLOCK();
        PUSH(cpu->registers.bc);
        CLOCK(END_OPCODE);
    case 0xC6: // ADD A, n (8 cycles)
        GET_OPERAND_8();
        CLOCK(add8(cpu, cpu->operand); END_OPCODE;);
    case 0xC7: // RST 0x00 (16 cycles)
        CLOCK();
        PUSH(cpu->registers.pc);
        CLOCK(cpu->registers.pc = 0x0000; END_OPCODE;);
    case 0xC8: // RET Z (8 or 20 cycles)
        RET_CC(!CHECK_FLAG(cpu, FLAG_Z))
    case 0xC9: // RET (16 cycles)
        POP(&cpu->registers.pc);
        CLOCK();
        CLOCK(END_OPCODE);
    case 0xCA: // JP Z, nn (12 or 16 cycles)
        GET_OPERAND_16();
        CLOCK(
            if (!CHECK_FLAG(cpu, FLAG_Z))
                END_OPCODE;
        );
        CLOCK(cpu->registers.pc = cpu->operand; END_OPCODE;);
    case 0xCB: // CB nn (prefix instruction) (4 cycles)
        GET_OPERAND_8(START_OPCODE_CB);
    case 0xCC: // CALL Z, nn (12 or 24 cycles)
        CALL_CC(!CHECK_FLAG(cpu, FLAG_Z));
    case 0xCD: // CALL nn (24 cycles)
        GET_OPERAND_16();
        CLOCK();
        PUSH(cpu->registers.pc);
        CLOCK(cpu->registers.pc = cpu->operand; END_OPCODE;);
    case 0xCE: // ADC A, n (8 cycles)
        GET_OPERAND_8();
        CLOCK(adc(cpu, cpu->operand); END_OPCODE;);
    case 0xCF: // RST 0x08 (16 cycles)
        CLOCK();
        PUSH(cpu->registers.pc);
        CLOCK(cpu->registers.pc = 0x0008; END_OPCODE;);
    case 0xD0: // RET NC (8 or 20 cycles)
        RET_CC(CHECK_FLAG(cpu, FLAG_C))
    case 0xD1: // POP DE (12 cycles)
        POP(&cpu->registers.de);
        CLOCK(END_OPCODE);
    case 0xD2: // JP NC, nn (12 or 16 cycles)
        GET_OPERAND_16();
        CLOCK(
            if (CHECK_FLAG(cpu, FLAG_C))
                END_OPCODE;
        );
        CLOCK(cpu->registers.pc = cpu->operand; END_OPCODE;);
    case 0xD4: // CALL NC, nn (12 or 24 cycles)
        CALL_CC(CHECK_FLAG(cpu, FLAG_C));
    case 0xD5: // PUSH DE (16 cycles)
        CLOCK();
        PUSH(cpu->registers.de);
        CLOCK(END_OPCODE);
    case 0xD6: // SUB A, n (8 cycles)
        GET_OPERAND_8();
        CLOCK(sub8(cpu, cpu->operand); END_OPCODE;);
    case 0xD7: // RST 0x10 (16 cycles)
        CLOCK();
        PUSH(cpu->registers.pc);
        CLOCK(cpu->registers.pc = 0x0010; END_OPCODE;);
    case 0xD8: // RET C (8 or 20 cycles)
        RET_CC(!CHECK_FLAG(cpu, FLAG_C));
    case 0xD9: // RETI (16 cycles)
        POP(&cpu->registers.pc);
        CLOCK(cpu->ime = IME_ENABLED);
        CLOCK(END_OPCODE);
    case 0xDA: // JP C, nn (12 or 16 cycles)
        GET_OPERAND_16();
        CLOCK(
            if (!CHECK_FLAG(cpu, FLAG_C))
                END_OPCODE
        );
        CLOCK(cpu->registers.pc = cpu->operand; END_OPCODE;);
    case 0xDC: // CALL C, nn (12 or 24 cycles)
        CALL_CC(!CHECK_FLAG(cpu, FLAG_C));
    case 0xDE: // SBC A, n (8 cycles)
        GET_OPERAND_8();
        CLOCK(sbc(cpu, cpu->operand); END_OPCODE;);
    case 0xDF: // RST 0x18 (16 cycles)
        CLOCK();
        PUSH(cpu->registers.pc);
        CLOCK(cpu->registers.pc = 0x0018; END_OPCODE;);
    case 0xE0: // LD (0xFF00 + n), A (12 cycles)
        GET_OPERAND_8();
        CLOCK(mmu_write(gb, IO + cpu->operand, cpu->registers.a));
        CLOCK(END_OPCODE);
    case 0xE1: // POP HL (12 cycles)
        POP(&cpu->registers.hl);
        CLOCK(END_OPCODE);
    case 0xE2: // LD (0xFF00 + C),A (8 cycles)
        CLOCK(mmu_write(gb, IO + cpu->registers.c, cpu->registers.a));
        CLOCK(END_OPCODE);
    case 0xE5: // PUSH HL (16 cycles)
        CLOCK();
        PUSH(cpu->registers.hl);
        CLOCK(END_OPCODE);
    case 0xE6: // AND n (8 cycles)
        GET_OPERAND_8();
        CLOCK(and(cpu, cpu->operand); END_OPCODE;);
    case 0xE7: // RST 0x20 (16 cycles)
        CLOCK();
        PUSH(cpu->registers.pc);
        CLOCK(cpu->registers.pc = 0x0020; END_OPCODE;);
    case 0xE8: // ADD SP, n (16 cycles)
        GET_OPERAND_8();
        CLOCK(cpu->accumulator = cpu->registers.sp + (s_byte_t) cpu->operand);
        CLOCK(
            (((cpu->registers.sp & 0xFF) + ((s_byte_t) cpu->operand & 0xFF)) & 0x100) == 0x100 ? SET_FLAG(cpu, FLAG_C) : RESET_FLAG(cpu, FLAG_C);
            (((cpu->registers.sp & 0x0F) + ((s_byte_t) cpu->operand & 0x0F)) & 0x10) == 0x10 ? SET_FLAG(cpu, FLAG_H) : RESET_FLAG(cpu, FLAG_H);
        );
        CLOCK(
            cpu->registers.sp = cpu->accumulator & 0xFFFF;
            RESET_FLAG(cpu, FLAG_N | FLAG_Z);
            END_OPCODE;
        );
    case 0xE9: // JP HL (4 cycles)
        CLOCK(cpu->registers.pc = cpu->registers.hl; END_OPCODE;);
    case 0xEA: // LD (nn), A (16 cycles)
        GET_OPERAND_16();
        CLOCK(mmu_write(gb, cpu->operand, cpu->registers.a));
        CLOCK(END_OPCODE);
    case 0xEE: // XOR n (8 cycles)
        GET_OPERAND_8();
        CLOCK(xor(cpu, cpu->operand); END_OPCODE;);
    case 0xEF: // RST 0x28 (16 cycles)
        CLOCK();
        PUSH(cpu->registers.pc);
        CLOCK(cpu->registers.pc = 0x0028; END_OPCODE;);
    case 0xF0: // LD A, (0xFF00 + n) (12 cycles)
        GET_OPERAND_8();
        CLOCK(cpu->accumulator = mmu_read(gb, IO + cpu->operand));
        CLOCK(cpu->registers.a = cpu->accumulator; END_OPCODE;);
    case 0xF1: // POP AF (12 cycles)
        // also clear lower nibble of cpu->registers.f because it can only retreive its flags (most significant nibble)
        POP(&cpu->registers.af);
        CLOCK(cpu->registers.f &= 0xF0; END_OPCODE;);
    case 0xF2: // LD A,(0xFF00 + C) (8 cycles)
        CLOCK(cpu->accumulator = mmu_read(gb, IO + cpu->registers.c););
        CLOCK(cpu->registers.a = cpu->accumulator; END_OPCODE;);
    case 0xF3: // DI (4 cycles)
        CLOCK(cpu->ime = IME_DISABLED; END_OPCODE;);
    case 0xF5: // PUSH AF (16 cycles)
        CLOCK();
        PUSH(cpu->registers.af);
        CLOCK(END_OPCODE);
    case 0xF6: // OR n (8 cycles)
        GET_OPERAND_8();
        CLOCK(or(cpu, cpu->operand); END_OPCODE;);
    case 0xF7: // RST 0x30 (16 cycles)
        CLOCK();
        PUSH(cpu->registers.pc);
        CLOCK(cpu->registers.pc = 0x0030; END_OPCODE;);
    case 0xF8: // LD HL, SP+n (12 cycles)
        GET_OPERAND_8();
        CLOCK(
            cpu->accumulator = cpu->registers.sp + (s_byte_t) cpu->operand;
            (((cpu->registers.sp & 0xFF) + ((s_byte_t) cpu->operand & 0xFF)) & 0x100) == 0x100 ? SET_FLAG(cpu, FLAG_C) : RESET_FLAG(cpu, FLAG_C);
            (((cpu->registers.sp & 0x0F) + ((s_byte_t) cpu->operand & 0x0F)) & 0x10) == 0x10 ? SET_FLAG(cpu, FLAG_H) : RESET_FLAG(cpu, FLAG_H);
        );
        CLOCK(
            cpu->registers.hl = cpu->accumulator & 0xFFFF;
            RESET_FLAG(cpu, FLAG_N | FLAG_Z);
            END_OPCODE;
        );
    case 0xF9: // LD SP, HL (8 cycles)
        CLOCK(cpu->registers.sp = cpu->registers.hl);
        CLOCK(END_OPCODE);
    case 0xFA: // LD A, (nn) (16 cycles)
        GET_OPERAND_16();
        CLOCK(cpu->accumulator = mmu_read(gb, cpu->operand));
        CLOCK(cpu->registers.a = cpu->accumulator; END_OPCODE;);
    case 0xFB: // EI (4 cycles)
        CLOCK(
            // If cpu->ime is not IME_DISABLED, either interrupts are in the process of being enabled, or they are already enabled.
            // In this case, don't do anything and let cpu->ime to either increase from IME_PENDING to IME_ENABLED (enabling interrupts)
            // or let it set to IME_ENABLED.
            if (cpu->ime == IME_DISABLED)
                cpu->ime = IME_PENDING;
            END_OPCODE;
        );
    case 0xFE: // CP n (8 cycles)
        GET_OPERAND_8();
        CLOCK(cp(cpu, cpu->operand); END_OPCODE;);
    case 0xFF: // RST 0x38 (16 cycles)
        CLOCK();
        PUSH(cpu->registers.pc);
        CLOCK(cpu->registers.pc = 0x0038; END_OPCODE;);
    default:
        CLOCK(
            eprintf("(invalid) opcode %02X\n", cpu->opcode);
            cpu->ime = IME_DISABLED;
            gb->cpu->halt = 1;
            END_OPCODE;
        );
        break;
    }
}

#ifdef DEBUG
static void print_trace(gb_t *gb) {
    gb_cpu_t *cpu = gb->cpu;

    byte_t opcode = mmu_read(gb, cpu->registers.pc);
    byte_t operand_size = instructions[opcode].operand_size;

    if (operand_size == 0) {
        printf("A:%02x F:%c%c%c%c BC:%04x DE:%04x HL:%04x SP:%04x PC:%04x | %02x        %s\n", cpu->registers.a, CHECK_FLAG(cpu, FLAG_Z) ? 'Z' : '-', CHECK_FLAG(cpu, FLAG_N) ? 'N' : '-', CHECK_FLAG(cpu, FLAG_H) ? 'H' : '-', CHECK_FLAG(cpu, FLAG_C) ? 'C' : '-', cpu->registers.bc, cpu->registers.de, cpu->registers.hl, cpu->registers.sp, cpu->registers.pc, opcode, instructions[opcode].name);
    } else if (operand_size == 1) {
        char buf[32];
        char *instr_name = opcode == 0xCB ? extended_instructions[mmu_read(gb, cpu->registers.pc + 1)].name : instructions[mmu_read(gb, cpu->registers.pc)].name;
        byte_t operand = mmu_read(gb, cpu->registers.pc + 1);
        snprintf(buf, sizeof(buf), instr_name, operand);
        printf("A:%02x F:%c%c%c%c BC:%04x DE:%04x HL:%04x SP:%04x PC:%04x | %02x %02x     %s\n", cpu->registers.a, CHECK_FLAG(cpu, FLAG_Z) ? 'Z' : '-', CHECK_FLAG(cpu, FLAG_N) ? 'N' : '-', CHECK_FLAG(cpu, FLAG_H) ? 'H' : '-', CHECK_FLAG(cpu, FLAG_C) ? 'C' : '-', cpu->registers.bc, cpu->registers.de, cpu->registers.hl, cpu->registers.sp, cpu->registers.pc, opcode, operand, buf);
    } else {
        char buf[32];
        byte_t first_operand = mmu_read(gb, cpu->registers.pc + 1);
        byte_t second_operand = mmu_read(gb, cpu->registers.pc + 2);
        word_t both_operands = first_operand | second_operand << 8;
        snprintf(buf, sizeof(buf), instructions[opcode].name, both_operands);
        printf("A:%02x F:%c%c%c%c BC:%04x DE:%04x HL:%04x SP:%04x PC:%04x | %02x %02x %02x  %s\n", cpu->registers.a, CHECK_FLAG(cpu, FLAG_Z) ? 'Z' : '-', CHECK_FLAG(cpu, FLAG_N) ? 'N' : '-', CHECK_FLAG(cpu, FLAG_H) ? 'H' : '-', CHECK_FLAG(cpu, FLAG_C) ? 'C' : '-', cpu->registers.bc, cpu->registers.de, cpu->registers.hl, cpu->registers.sp, cpu->registers.pc, opcode, first_operand, second_operand, buf);
    }
}
#endif

static void push_interrupt(gb_t *gb) {
    gb_cpu_t *cpu = gb->cpu;
    gb_mmu_t *mmu = gb->mmu;

    switch (cpu->opcode_state) {
    case 0:
        CLOCK();
        CLOCK();
        CLOCK();
        CLOCK(mmu_write(gb, --cpu->registers.sp, (cpu->registers.pc) >> 8));
        CLOCK(
            byte_t old_ie = mmu->ie; // in case the mmu_write below overwrites the IE register
            mmu_write(gb, --cpu->registers.sp, cpu->registers.pc & 0xFF);
            if (CHECK_BIT(mmu->io_registers[IF - IO], IRQ_VBLANK) && CHECK_BIT(old_ie, IRQ_VBLANK)) {
                RESET_BIT(mmu->io_registers[IF - IO], IRQ_VBLANK);
                cpu->registers.pc = 0x0040;
            } else if (CHECK_BIT(mmu->io_registers[IF - IO], IRQ_STAT) && CHECK_BIT(old_ie, IRQ_STAT)) {
                RESET_BIT(mmu->io_registers[IF - IO], IRQ_STAT);
                cpu->registers.pc = 0x0048;
            } else if (CHECK_BIT(mmu->io_registers[IF - IO], IRQ_TIMER) && CHECK_BIT(old_ie, IRQ_TIMER)) {
                RESET_BIT(mmu->io_registers[IF - IO], IRQ_TIMER);
                cpu->registers.pc = 0x0050;
            } else if (CHECK_BIT(mmu->io_registers[IF - IO], IRQ_SERIAL) && CHECK_BIT(old_ie, IRQ_SERIAL)) {
                RESET_BIT(mmu->io_registers[IF - IO], IRQ_SERIAL);
                cpu->registers.pc = 0x0058;
            } else if (CHECK_BIT(mmu->io_registers[IF - IO], IRQ_JOYPAD) && CHECK_BIT(old_ie, IRQ_JOYPAD)) {
                RESET_BIT(mmu->io_registers[IF - IO], IRQ_JOYPAD);
                cpu->registers.pc = 0x0060;
            } else {
                // an overwrite of the IE register happened during the previous CLOCK() and disabled all interrupts
                // this has the effect to jump the cpu to address 0x0000
                cpu->registers.pc = 0x0000;
            }
            END_PUSH_IRQ;
        );
    }
}

void cpu_step(gb_t *gb) {
    gb_cpu_t *cpu = gb->cpu;

    switch (cpu->exec_state) {
    case FETCH_OPCODE:
        if (cpu->halt) {
            if (IS_INTERRUPT_PENDING(gb))
                cpu->halt = 0; // interrupt requested, disabling halt takes 4 cycles
            return;
        }

        if (cpu->ime == IME_PENDING) {
            cpu->ime = IME_ENABLED;
        } else if (cpu->ime == IME_ENABLED && IS_INTERRUPT_PENDING(gb)) {
            cpu->opcode = 0;
            cpu->opcode_state = cpu->opcode;
            cpu->exec_state = EXEC_PUSH_IRQ;
            push_interrupt(gb);
            break;
        }

        #ifdef DEBUG
        print_trace(gb);
        #endif

        cpu->opcode = mmu_read(gb, cpu->registers.pc);
        cpu->opcode_state = cpu->opcode;
        if (cpu->halt_bug)
            cpu->halt_bug = 0;
        else
            cpu->registers.pc++;
        cpu->exec_state = EXEC_OPCODE;
        // exec opcode now
        // fall through
    case EXEC_OPCODE:
        exec_opcode(gb);
        break;
    case FETCH_OPCODE_CB:
        cpu->opcode = cpu->operand;
        cpu->opcode_state = cpu->opcode;
        cpu->exec_state = EXEC_OPCODE_CB;
        // exec extended opcode now
        // fall through
    case EXEC_OPCODE_CB:
        exec_extended_opcode(gb);
        break;
    case EXEC_PUSH_IRQ:
        push_interrupt(gb);
        break;
    }
}

void cpu_init(gb_t *gb) {
    gb->cpu = xcalloc(1, sizeof(*gb->cpu));
    gb->cpu->exec_state = FETCH_OPCODE; // immediately request to fetch an instruction
}

void cpu_quit(gb_t *gb) {
    free(gb->cpu);
}

#define SERIALIZED_MEMBERS \
    X(registers.af)        \
    X(registers.bc)        \
    X(registers.de)        \
    X(registers.hl)        \
    X(registers.sp)        \
    X(registers.pc)        \
    X(ime)                 \
    X(halt)                \
    X(halt_bug)            \
    X(exec_state)          \
    X(opcode)              \
    X(opcode_state)        \
    X(operand)             \
    X(accumulator)

#define X(value) SERIALIZED_LENGTH(value);
SERIALIZED_SIZE_FUNCTION(gb_cpu_t, cpu,
    SERIALIZED_MEMBERS
)
#undef X

#define X(value) SERIALIZE(value);
SERIALIZER_FUNCTION(gb_cpu_t, cpu,
    SERIALIZED_MEMBERS
)
#undef X

#define X(value) UNSERIALIZE(value);
UNSERIALIZER_FUNCTION(gb_cpu_t, cpu,
    SERIALIZED_MEMBERS
)
#undef X
