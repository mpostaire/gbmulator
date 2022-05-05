#include <stdio.h>
#include <stdlib.h>

#include "utils.h"
#include "cpu.h"
#include "mmu.h"

struct instruction {
    char *name;
    int operand_size;
};

cpu_t cpu;

const struct instruction instructions[256] = {
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

const struct instruction extended_instructions[256] = {
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
    {"BIT 6, H", 0},    // 0x6C
    {"BIT 6, L", 0},    // 0x6D
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

static void push(word_t word) {
    byte_t hi = word >> 8;
    byte_t lo = word & 0xFF;
    cpu.registers.sp--;
    mmu_write(cpu.registers.sp, hi);
    cpu.registers.sp--;
    mmu_write(cpu.registers.sp, lo);
}

static word_t pop(void) {
    word_t data = mmu_read(cpu.registers.sp) | mmu_read(cpu.registers.sp + 1) << 8;
    cpu.registers.sp += 2;
    return data;
}

static void and(byte_t reg) {
    cpu.registers.a &= reg;
    cpu.registers.a ? RESET_FLAG(FLAG_Z) : SET_FLAG(FLAG_Z);
    RESET_FLAG(FLAG_N | FLAG_C);
    SET_FLAG(FLAG_H);
}

static void or(byte_t reg) {
    cpu.registers.a |= reg;
    cpu.registers.a ? RESET_FLAG(FLAG_Z) : SET_FLAG(FLAG_Z);
    RESET_FLAG(FLAG_C | FLAG_H | FLAG_N);
}

static void xor(byte_t reg) {
    cpu.registers.a ^= reg;
    cpu.registers.a ? RESET_FLAG(FLAG_Z) : SET_FLAG(FLAG_Z);
    RESET_FLAG(FLAG_C | FLAG_H | FLAG_N);
}

static void bit(byte_t reg, byte_t pos) {
    CHECK_BIT(reg, pos) ? RESET_FLAG(FLAG_Z) : SET_FLAG(FLAG_Z);
    RESET_FLAG(FLAG_N);
    SET_FLAG(FLAG_H);
}

static byte_t inc(byte_t reg) {
    // if lower nibble of reg is at max value, there will be a half carry after we increment.
    (reg & 0x0F) == 0x0F ? SET_FLAG(FLAG_H) : RESET_FLAG(FLAG_H);
    reg++;
    reg ? RESET_FLAG(FLAG_Z) : SET_FLAG(FLAG_Z);
    RESET_FLAG(FLAG_N);
    return reg;
}

static byte_t dec(byte_t reg) {
    // if lower nibble of reg is 0, there will be a half carry after we decrement (not sure if this is a sufficient condition)
    (reg & 0x0F) == 0x00 ? SET_FLAG(FLAG_H) : RESET_FLAG(FLAG_H);
    reg--;
    reg ? RESET_FLAG(FLAG_Z) : SET_FLAG(FLAG_Z);
    SET_FLAG(FLAG_N);
    return reg;
}

static byte_t rl(byte_t reg) {
    int carry = CHECK_FLAG(FLAG_C) ? 1 : 0;
    // set new carry to the value of leftmost bit
    (reg & 0x80) ? SET_FLAG(FLAG_C) : RESET_FLAG(FLAG_C);
    // shift left
    reg <<= 1;
    // set rightmost bit to the value of the old carry
    reg += carry;

    reg ? RESET_FLAG(FLAG_Z) : SET_FLAG(FLAG_Z);
    RESET_FLAG(FLAG_N | FLAG_H);
    return reg;
}

static byte_t rlc(byte_t reg) {
    // set new carry to the value of leftmost bit
    (reg & 0x80) ? SET_FLAG(FLAG_C) : RESET_FLAG(FLAG_C);
    // shift left
    reg <<= 1;
    // set rightmost bit to the value of the carry (previous bit 7 value)
    reg += CHECK_FLAG(FLAG_C) ? 1 : 0;

    reg ? RESET_FLAG(FLAG_Z) : SET_FLAG(FLAG_Z);
    RESET_FLAG(FLAG_N | FLAG_H);
    return reg;
}

static byte_t srl(byte_t reg) {
    // set carry to the value of rightmost bit
    (reg & 0x01) ? SET_FLAG(FLAG_C) : RESET_FLAG(FLAG_C);
    // shift right
    reg >>= 1;

    reg ? RESET_FLAG(FLAG_Z) : SET_FLAG(FLAG_Z);
    RESET_FLAG(FLAG_N | FLAG_H);
    return reg;
}

static byte_t sla(byte_t reg) {
    // set carry to the value of leftmost bit
    (reg & 0x80) ? SET_FLAG(FLAG_C) : RESET_FLAG(FLAG_C);
    // shift left
    reg <<= 1;

    reg ? RESET_FLAG(FLAG_Z) : SET_FLAG(FLAG_Z);
    RESET_FLAG(FLAG_N | FLAG_H);
    return reg;
}

static byte_t sra(byte_t reg) {
    // set carry to the value of rightmost bit
    int msb = reg & 0x80;
    (reg & 0x01) ? SET_FLAG(FLAG_C) : RESET_FLAG(FLAG_C);
    // shift right
    reg >>= 1;
    reg |= msb;

    reg ? RESET_FLAG(FLAG_Z) : SET_FLAG(FLAG_Z);
    RESET_FLAG(FLAG_N | FLAG_H);
    return reg;
}

static byte_t rr(byte_t reg) {
    int carry = CHECK_FLAG(FLAG_C) ? 1 : 0;
    // set new carry to the value of rightmost bit
    (reg & 0x01) ? SET_FLAG(FLAG_C) : RESET_FLAG(FLAG_C);
    // shift right
    reg >>= 1;
    // set leftmost bit to the value of the old carry
    carry ? SET_BIT(reg, 7) : RESET_BIT(reg, 7);

    reg ? RESET_FLAG(FLAG_Z) : SET_FLAG(FLAG_Z);
    RESET_FLAG(FLAG_N | FLAG_H);
    return reg;
}

static byte_t rrc(byte_t reg) {
    // set new carry to the value of rightmost bit
    (reg & 0x01) ? SET_FLAG(FLAG_C) : RESET_FLAG(FLAG_C);
    // shift right
    reg >>= 1;
    // set leftmost bit to the value of the carry (previous bit 0 value)
    CHECK_FLAG(FLAG_C) ? SET_BIT(reg, 7) : RESET_BIT(reg, 7);

    reg ? RESET_FLAG(FLAG_Z) : SET_FLAG(FLAG_Z);
    RESET_FLAG(FLAG_N | FLAG_H);
    return reg;
}

static byte_t swap(byte_t reg) {
    reg = (reg << 4) | (reg >> 4);
    reg ? RESET_FLAG(FLAG_Z) : SET_FLAG(FLAG_Z);
    RESET_FLAG(FLAG_C | FLAG_H | FLAG_N);
    return reg;
}

static void cp(byte_t reg) {
    cpu.registers.a == reg ? SET_FLAG(FLAG_Z) : RESET_FLAG(FLAG_Z);
    reg > cpu.registers.a ? SET_FLAG(FLAG_C) : RESET_FLAG(FLAG_C);
    (reg & 0x0F) > (cpu.registers.a & 0x0F) ? SET_FLAG(FLAG_H) : RESET_FLAG(FLAG_H);
    SET_FLAG(FLAG_N);
}

static void add8(byte_t val) {
    unsigned int result = cpu.registers.a + val;
    (result & 0xFF00) ? SET_FLAG(FLAG_C) : RESET_FLAG(FLAG_C);
    (((cpu.registers.a & 0x0F) + (val & 0x0F)) & 0x10) == 0x10 ? SET_FLAG(FLAG_H) : RESET_FLAG(FLAG_H);
    cpu.registers.a = result & 0xFF;
    cpu.registers.a ? RESET_FLAG(FLAG_Z) : SET_FLAG(FLAG_Z);
    RESET_FLAG(FLAG_N);
}

static void add16(word_t val) {
    unsigned int result = cpu.registers.hl + val;
    (result & 0xFFFF0000) ? SET_FLAG(FLAG_C) : RESET_FLAG(FLAG_C);
    (((cpu.registers.hl & 0x0FFF) + (val & 0x0FFF)) & 0x1000) == 0x1000 ? SET_FLAG(FLAG_H) : RESET_FLAG(FLAG_H);
    cpu.registers.hl = result & 0xFFFF;
    RESET_FLAG(FLAG_N);
}

static void adc(byte_t val) {
    byte_t c = CHECK_FLAG(FLAG_C) ? 1 : 0;
    unsigned int result = cpu.registers.a + val + c;
    (result & 0xFF00) ? SET_FLAG(FLAG_C) : RESET_FLAG(FLAG_C);
    (((cpu.registers.a & 0x0F) + (val & 0x0F) + c) & 0x10) == 0x10 ? SET_FLAG(FLAG_H) : RESET_FLAG(FLAG_H);
    cpu.registers.a = result & 0xFF;
    cpu.registers.a ? RESET_FLAG(FLAG_Z) : SET_FLAG(FLAG_Z);
    RESET_FLAG(FLAG_N);
}

static void sub8(byte_t reg) {
    reg > cpu.registers.a ? SET_FLAG(FLAG_C) : RESET_FLAG(FLAG_C);
    (reg & 0x0F) > (cpu.registers.a & 0x0F) ? SET_FLAG(FLAG_H) : RESET_FLAG(FLAG_H);
    cpu.registers.a -= reg;
    cpu.registers.a ? RESET_FLAG(FLAG_Z) : SET_FLAG(FLAG_Z);
    SET_FLAG(FLAG_N);
}

static void sbc(byte_t reg) {
    byte_t c = CHECK_FLAG(FLAG_C) ? 1 : 0;
    reg + c > cpu.registers.a ? SET_FLAG(FLAG_C) : RESET_FLAG(FLAG_C);
    ((reg & 0x0F) + c) > (cpu.registers.a & 0x0F) ? SET_FLAG(FLAG_H) : RESET_FLAG(FLAG_H);
    cpu.registers.a -= reg + c;
    cpu.registers.a ? RESET_FLAG(FLAG_Z) : SET_FLAG(FLAG_Z);
    SET_FLAG(FLAG_N);
}

static int exec_extended_opcode(byte_t opcode) {
    int cycles;
    byte_t temp; // temporary variable used for some opcodes

    switch (opcode) {
    case 0x00: // RLC B
        cpu.registers.b = rlc(cpu.registers.b);
        cycles = 8;
        break;
    case 0x01: // RLC C
        cpu.registers.c = rlc(cpu.registers.c);
        cycles = 8;
        break;
    case 0x02: // RLC D
        cpu.registers.d = rlc(cpu.registers.d);
        cycles = 8;
        break;
    case 0x03: // RLC E
        cpu.registers.e = rlc(cpu.registers.e);
        cycles = 8;
        break;
    case 0x04: // RLC H
        cpu.registers.h = rlc(cpu.registers.h);
        cycles = 8;
        break;
    case 0x05: // RLC L
        cpu.registers.l = rlc(cpu.registers.l);
        cycles = 8;
        break;
    case 0x06: // RLC (HL)
        mmu_write(cpu.registers.hl, rlc(mmu_read(cpu.registers.hl)));
        cycles = 16;
        break;
    case 0x07: // RLC A
        cpu.registers.a = rlc(cpu.registers.a);
        cycles = 8;
        break;
    case 0x08: // RRC B
        cpu.registers.b = rrc(cpu.registers.b);
        cycles = 8;
        break;
    case 0x09: // RRC C
        cpu.registers.c = rrc(cpu.registers.c);
        cycles = 8;
        break;
    case 0x0A: // RRC D
        cpu.registers.d = rrc(cpu.registers.d);
        cycles = 8;
        break;
    case 0x0B: // RRC E
        cpu.registers.e = rrc(cpu.registers.e);
        cycles = 8;
        break;
    case 0x0C: // RRC H
        cpu.registers.h = rrc(cpu.registers.h);
        cycles = 8;
        break;
    case 0x0D: // RRC L
        cpu.registers.l = rrc(cpu.registers.l);
        cycles = 8;
        break;
    case 0x0E: // RRC (HL)
        mmu_write(cpu.registers.hl, rrc(mmu_read(cpu.registers.hl)));
        cycles = 16;
        break;
    case 0x0F: // RRC A
        cpu.registers.a = rrc(cpu.registers.a);
        cycles = 8;
        break;
    case 0x10: // RL B
        cpu.registers.b = rl(cpu.registers.b);
        cycles = 8;
        break;
    case 0x11: // RL C
        cpu.registers.c = rl(cpu.registers.c);
        cycles = 8;
        break;
    case 0x12: // RL D
        cpu.registers.d = rl(cpu.registers.d);
        cycles = 8;
        break;
    case 0x13: // RL E
        cpu.registers.e = rl(cpu.registers.e);
        cycles = 8;
        break;
    case 0x14: // RL H
        cpu.registers.h = rl(cpu.registers.h);
        cycles = 8;
        break;
    case 0x15: // RL L
        cpu.registers.l = rl(cpu.registers.l);
        cycles = 8;
        break;
    case 0x16: // RL (HL)
        mmu_write(cpu.registers.hl, rl(mmu_read(cpu.registers.hl)));
        cycles = 16;
        break;
    case 0x17: // RL A
        cpu.registers.a = rl(cpu.registers.a);
        cycles = 8;
        break;
    case 0x18: // RR B
        cpu.registers.b = rr(cpu.registers.b);
        cycles = 8;
        break;
    case 0x19: // RR C
        cpu.registers.c = rr(cpu.registers.c);
        cycles = 8;
        break;
    case 0x1A: // RR D
        cpu.registers.d = rr(cpu.registers.d);
        cycles = 8;
        break;
    case 0x1B: // RR E
        cpu.registers.e = rr(cpu.registers.e);
        cycles = 8;
        break;
    case 0x1C: // RR H
        cpu.registers.h = rr(cpu.registers.h);
        cycles = 8;
        break;
    case 0x1D: // RR L
        cpu.registers.l = rr(cpu.registers.l);
        cycles = 8;
        break;
    case 0x1E: // RR (HL)
        mmu_write(cpu.registers.hl, rr(mmu_read(cpu.registers.hl)));
        cycles = 16;
        break;
    case 0x1F: // RR A
        cpu.registers.a = rr(cpu.registers.a);
        cycles = 8;
        break;
    case 0x20: // SLA B
        cpu.registers.b = sla(cpu.registers.b);
        cycles = 8;
        break;
    case 0x21: // SLA C
        cpu.registers.c = sla(cpu.registers.c);
        cycles = 8;
        break;
    case 0x22: // SLA D
        cpu.registers.d = sla(cpu.registers.d);
        cycles = 8;
        break;
    case 0x23: // SLA E
        cpu.registers.e = sla(cpu.registers.e);
        cycles = 8;
        break;
    case 0x24: // SLA H
        cpu.registers.h = sla(cpu.registers.h);
        cycles = 8;
        break;
    case 0x25: // SLA L
        cpu.registers.l = sla(cpu.registers.l);
        cycles = 8;
        break;
    case 0x26: // SLA (HL)
        mmu_write(cpu.registers.hl, sla(mmu_read(cpu.registers.hl)));
        cycles = 16;
        break;
    case 0x27: // SLA A
        cpu.registers.a = sla(cpu.registers.a);
        cycles = 8;
        break;
    case 0x28: // SRA B
        cpu.registers.b = sra(cpu.registers.b);
        cycles = 8;
        break;
    case 0x29: // SRA C
        cpu.registers.c = sra(cpu.registers.c);
        cycles = 8;
        break;
    case 0x2A: // SRA D
        cpu.registers.d = sra(cpu.registers.d);
        cycles = 8;
        break;
    case 0x2B: // SRA E
        cpu.registers.e = sra(cpu.registers.e);
        cycles = 8;
        break;
    case 0x2C: // SRA H
        cpu.registers.h = sra(cpu.registers.h);
        cycles = 8;
        break;
    case 0x2D: // SRA L
        cpu.registers.l = sra(cpu.registers.l);
        cycles = 8;
        break;
    case 0x2E: // SRA (HL)
        mmu_write(cpu.registers.hl, sra(mmu_read(cpu.registers.hl)));
        cycles = 16;
        break;
    case 0x2F: // SRA A
        cpu.registers.a = sra(cpu.registers.a);
        cycles = 8;
        break;
    case 0x30: // SWAP B
        cpu.registers.b = swap(cpu.registers.b);
        cycles = 8;
        break;
    case 0x31: // SWAP C
        cpu.registers.c = swap(cpu.registers.c);
        cycles = 8;
        break;
    case 0x32: // SWAP D
        cpu.registers.d = swap(cpu.registers.d);
        cycles = 8;
        break;
    case 0x33: // SWAP E
        cpu.registers.e = swap(cpu.registers.e);
        cycles = 8;
        break;
    case 0x34: // SWAP H
        cpu.registers.h = swap(cpu.registers.h);
        cycles = 8;
        break;
    case 0x35: // SWAP L
        cpu.registers.l = swap(cpu.registers.l);
        cycles = 8;
        break;
    case 0x36: // SWAP (HL)
        mmu_write(cpu.registers.hl, swap(mmu_read(cpu.registers.hl)));
        cycles = 16;
        break;
    case 0x37: // SWAP A
        cpu.registers.a = swap(cpu.registers.a);
        cycles = 8;
        break;
    case 0x38: // SRL B
        cpu.registers.b = srl(cpu.registers.b);
        cycles = 8;
        break;
    case 0x39: // SRL C
        cpu.registers.c = srl(cpu.registers.c);
        cycles = 8;
        break;
    case 0x3A: // SRL D
        cpu.registers.d = srl(cpu.registers.d);
        cycles = 8;
        break;
    case 0x3B: // SRL E
        cpu.registers.e = srl(cpu.registers.e);
        cycles = 8;
        break;
    case 0x3C: // SRL H
        cpu.registers.h = srl(cpu.registers.h);
        cycles = 8;
        break;
    case 0x3D: // SRL L
        cpu.registers.l = srl(cpu.registers.l);
        cycles = 8;
        break;
    case 0x3E: // SRL (HL)
        mmu_write(cpu.registers.hl, srl(mmu_read(cpu.registers.hl)));
        cycles = 16;
        break;
    case 0x3F: // SRL A
        cpu.registers.a = srl(cpu.registers.a);
        cycles = 8;
        break;
    case 0x40: // BIT 0, B
        bit(cpu.registers.b, 0);
        cycles = 8;
        break;
    case 0x41: // BIT 0, C
        bit(cpu.registers.c, 0);
        cycles = 8;
        break;
    case 0x42: // BIT 0, D
        bit(cpu.registers.d, 0);
        cycles = 8;
        break;
    case 0x43: // BIT 0, E
        bit(cpu.registers.e, 0);
        cycles = 8;
        break;
    case 0x44: // BIT 0, H
        bit(cpu.registers.h, 0);
        cycles = 8;
        break;
    case 0x45: // BIT 0, L
        bit(cpu.registers.l, 0);
        cycles = 8;
        break;
    case 0x46: // BIT 0, (HL)
        bit(mmu_read(cpu.registers.hl), 0);
        cycles = 12;
        break;
    case 0x47: // BIT 0, A
        bit(cpu.registers.a, 0);
        cycles = 8;
        break;
    case 0x48: // BIT 1, B
        bit(cpu.registers.b, 1);
        cycles = 8;
        break;
    case 0x49: // BIT 1, C
        bit(cpu.registers.c, 1);
        cycles = 8;
        break;
    case 0x4A: // BIT 1, D
        bit(cpu.registers.d, 1);
        cycles = 8;
        break;
    case 0x4B: // BIT 1, E
        bit(cpu.registers.e, 1);
        cycles = 8;
        break;
    case 0x4C: // BIT 1, H
        bit(cpu.registers.h, 1);
        cycles = 8;
        break;
    case 0x4D: // BIT 1, L
        bit(cpu.registers.l, 1);
        cycles = 8;
        break;
    case 0x4E: // BIT 1, (HL)
        bit(mmu_read(cpu.registers.hl), 1);
        cycles = 12;
        break;
    case 0x4F: // BIT 1, A
        bit(cpu.registers.a, 1);
        cycles = 8;
        break;
    case 0x50: // BIT 2, B
        bit(cpu.registers.b, 2);
        cycles = 8;
        break;
    case 0x51: // BIT 2, C
        bit(cpu.registers.c, 2);
        cycles = 8;
        break;
    case 0x52: // BIT 2, D
        bit(cpu.registers.d, 2);
        cycles = 8;
        break;
    case 0x53: // BIT 2, E
        bit(cpu.registers.e, 2);
        cycles = 8;
        break;
    case 0x54: // BIT 2, H
        bit(cpu.registers.h, 2);
        cycles = 8;
        break;
    case 0x55: // BIT 2, L
        bit(cpu.registers.l, 2);
        cycles = 8;
        break;
    case 0x56: // BIT 2, (HL)
        bit(mmu_read(cpu.registers.hl), 2);
        cycles = 12;
        break;
    case 0x57: // BIT 2, A
        bit(cpu.registers.a, 2);
        cycles = 8;
        break;
    case 0x58: // BIT 3, B
        bit(cpu.registers.b, 3);
        cycles = 8;
        break;
    case 0x59: // BIT 3, C
        bit(cpu.registers.c, 3);
        cycles = 8;
        break;
    case 0x5A: // BIT 3, D
        bit(cpu.registers.d, 3);
        cycles = 8;
        break;
    case 0x5B: // BIT 3, E
        bit(cpu.registers.e, 3);
        cycles = 8;
        break;
    case 0x5C: // BIT 3, H
        bit(cpu.registers.h, 3);
        cycles = 8;
        break;
    case 0x5D: // BIT 3, L
        bit(cpu.registers.l, 3);
        cycles = 8;
        break;
    case 0x5E: // BIT 3, (HL)
        bit(mmu_read(cpu.registers.hl), 3);
        cycles = 12;
        break;
    case 0x5F: // BIT 3, A
        bit(cpu.registers.a, 3);
        cycles = 8;
        break;
    case 0x60: // BIT 4, B
        bit(cpu.registers.b, 4);
        cycles = 8;
        break;
    case 0x61: // BIT 4, C
        bit(cpu.registers.c, 4);
        cycles = 8;
        break;
    case 0x62: // BIT 4, D
        bit(cpu.registers.d, 4);
        cycles = 8;
        break;
    case 0x63: // BIT 4, E
        bit(cpu.registers.e, 4);
        cycles = 8;
        break;
    case 0x64: // BIT 4, H
        bit(cpu.registers.h, 4);
        cycles = 8;
        break;
    case 0x65: // BIT 4, L
        bit(cpu.registers.l, 4);
        cycles = 8;
        break;
    case 0x66: // BIT 4, (HL)
        bit(mmu_read(cpu.registers.hl), 4);
        cycles = 12;
        break;
    case 0x67: // BIT 4, A
        bit(cpu.registers.a, 4);
        cycles = 8;
        break;
    case 0x68: // BIT 5, B
        bit(cpu.registers.b, 5);
        cycles = 8;
        break;
    case 0x69: // BIT 5, C
        bit(cpu.registers.c, 5);
        cycles = 8;
        break;
    case 0x6A: // BIT 5, D
        bit(cpu.registers.d, 5);
        cycles = 8;
        break;
    case 0x6B: // BIT 5, E
        bit(cpu.registers.e, 5);
        cycles = 8;
        break;
    case 0x6C: // BIT 5, H
        bit(cpu.registers.h, 5);
        cycles = 8;
        break;
    case 0x6D: // BIT 5, L
        bit(cpu.registers.l, 5);
        cycles = 8;
        break;
    case 0x6E: // BIT 5, (HL)
        bit(mmu_read(cpu.registers.hl), 5);
        cycles = 12;
        break;
    case 0x6F: // BIT 5, A
        bit(cpu.registers.a, 5);
        cycles = 8;
        break;
    case 0x70: // BIT 6, B
        bit(cpu.registers.b, 6);
        cycles = 8;
        break;
    case 0x71: // BIT 6, C
        bit(cpu.registers.c, 6);
        cycles = 8;
        break;
    case 0x72: // BIT 6, D
        bit(cpu.registers.d, 6);
        cycles = 8;
        break;
    case 0x73: // BIT 6, E
        bit(cpu.registers.e, 6);
        cycles = 8;
        break;
    case 0x74: // BIT 6, H
        bit(cpu.registers.h, 6);
        cycles = 8;
        break;
    case 0x75: // BIT 6, L
        bit(cpu.registers.l, 6);
        cycles = 8;
        break;
    case 0x76: // BIT 6, (HL)
        bit(mmu_read(cpu.registers.hl), 6);
        cycles = 12;
        break;
    case 0x77: // BIT 6, A
        bit(cpu.registers.a, 6);
        cycles = 8;
        break;
    case 0x78: // BIT 7, B
        bit(cpu.registers.b, 7);
        cycles = 8;
        break;
    case 0x79: // BIT 7, C
        bit(cpu.registers.c, 7);
        cycles = 8;
        break;
    case 0x7A: // BIT 7, D
        bit(cpu.registers.d, 7);
        cycles = 8;
        break;
    case 0x7B: // BIT 7, E
        bit(cpu.registers.e, 7);
        cycles = 8;
        break;
    case 0x7C: // BIT 7, H
        bit(cpu.registers.h, 7);
        cycles = 8;
        break;
    case 0x7D: // BIT 7, L
        bit(cpu.registers.l, 7);
        cycles = 8;
        break;
    case 0x7E: // BIT 7, (HL)
        bit(mmu_read(cpu.registers.hl), 7);
        cycles = 12;
        break;
    case 0x7F: // BIT 7, A
        bit(cpu.registers.a, 7);
        cycles = 8;
        break;
    case 0x80: // RES 0, B
        RESET_BIT(cpu.registers.b, 0);
        cycles = 8;
        break;
    case 0x81: // RES 0, C
        RESET_BIT(cpu.registers.c, 0);
        cycles = 8;
        break;
    case 0x82: // RES 0, D
        RESET_BIT(cpu.registers.d, 0);
        cycles = 8;
        break;
    case 0x83: // RES 0, E
        RESET_BIT(cpu.registers.e, 0);
        cycles = 8;
        break;
    case 0x84: // RES 0, H
        RESET_BIT(cpu.registers.h, 0);
        cycles = 8;
        break;
    case 0x85: // RES 0, L
        RESET_BIT(cpu.registers.l, 0);
        cycles = 8;
        break;
    case 0x86: // RES 0, (HL)
        temp = mmu_read(cpu.registers.hl);
        mmu_write(cpu.registers.hl, RESET_BIT(temp, 0));
        cycles = 16;
        break;
    case 0x87: // RES 0, A
        RESET_BIT(cpu.registers.a, 0);
        cycles = 8;
        break;
    case 0x88: // RES 1, B
        RESET_BIT(cpu.registers.b, 1);
        cycles = 8;
        break;
    case 0x89: // RES 1, C
        RESET_BIT(cpu.registers.c, 1);
        cycles = 8;
        break;
    case 0x8A: // RES 1, D
        RESET_BIT(cpu.registers.d, 1);
        cycles = 8;
        break;
    case 0x8B: // RES 1, E
        RESET_BIT(cpu.registers.e, 1);
        cycles = 8;
        break;
    case 0x8C: // RES 1, H
        RESET_BIT(cpu.registers.h, 1);
        cycles = 8;
        break;
    case 0x8D: // RES 1, L
        RESET_BIT(cpu.registers.l, 1);
        cycles = 8;
        break;
    case 0x8E: // RES 1, (HL)
        temp = mmu_read(cpu.registers.hl);
        mmu_write(cpu.registers.hl, RESET_BIT(temp, 1));
        cycles = 16;
        break;
    case 0x8F: // RES 1, A
        RESET_BIT(cpu.registers.a, 1);
        cycles = 8;
        break;
    case 0x90: // RES 2, B
        RESET_BIT(cpu.registers.b, 2);
        cycles = 8;
        break;
    case 0x91: // RES 2, C
        RESET_BIT(cpu.registers.c, 2);
        cycles = 8;
        break;
    case 0x92: // RES 2, D
        RESET_BIT(cpu.registers.d, 2);
        cycles = 8;
        break;
    case 0x93: // RES 2, E
        RESET_BIT(cpu.registers.e, 2);
        cycles = 8;
        break;
    case 0x94: // RES 2, H
        RESET_BIT(cpu.registers.h, 2);
        cycles = 8;
        break;
    case 0x95: // RES 2, L
        RESET_BIT(cpu.registers.l, 2);
        cycles = 8;
        break;
    case 0x96: // RES 2, (HL)
        temp = mmu_read(cpu.registers.hl);
        mmu_write(cpu.registers.hl, RESET_BIT(temp, 2));
        cycles = 16;
        break;
    case 0x97: // RES 2, A
        RESET_BIT(cpu.registers.a, 2);
        cycles = 8;
        break;
    case 0x98: // RES 3, B
        RESET_BIT(cpu.registers.b, 3);
        cycles = 8;
        break;
    case 0x99: // RES 3, C
        RESET_BIT(cpu.registers.c, 3);
        cycles = 8;
        break;
    case 0x9A: // RES 3, D
        RESET_BIT(cpu.registers.d, 3);
        cycles = 8;
        break;
    case 0x9B: // RES 3, E
        RESET_BIT(cpu.registers.e, 3);
        cycles = 8;
        break;
    case 0x9C: // RES 3, H
        RESET_BIT(cpu.registers.h, 3);
        cycles = 8;
        break;
    case 0x9D: // RES 3, L
        RESET_BIT(cpu.registers.l, 3);
        cycles = 8;
        break;
    case 0x9E: // RES 3, (HL)
        temp = mmu_read(cpu.registers.hl);
        mmu_write(cpu.registers.hl, RESET_BIT(temp, 3));
        cycles = 16;
        break;
    case 0x9F: // RES 3, A
        RESET_BIT(cpu.registers.a, 3);
        cycles = 8;
        break;
    case 0xA0: // RES 4, B
        RESET_BIT(cpu.registers.b, 4);
        cycles = 8;
        break;
    case 0xA1: // RES 4, C
        RESET_BIT(cpu.registers.c, 4);
        cycles = 8;
        break;
    case 0xA2: // RES 4, D
        RESET_BIT(cpu.registers.d, 4);
        cycles = 8;
        break;
    case 0xA3: // RES 4, E
        RESET_BIT(cpu.registers.e, 4);
        cycles = 8;
        break;
    case 0xA4: // RES 4, H
        RESET_BIT(cpu.registers.h, 4);
        cycles = 8;
        break;
    case 0xA5: // RES 4, L
        RESET_BIT(cpu.registers.l, 4);
        cycles = 8;
        break;
    case 0xA6: // RES 4, (HL)
        temp = mmu_read(cpu.registers.hl);
        mmu_write(cpu.registers.hl, RESET_BIT(temp, 4));
        cycles = 16;
        break;
    case 0xA7: // RES 4, A
        RESET_BIT(cpu.registers.a, 4);
        cycles = 8;
        break;
    case 0xA8: // RES 5, B
        RESET_BIT(cpu.registers.b, 5);
        cycles = 8;
        break;
    case 0xA9: // RES 5, C
        RESET_BIT(cpu.registers.c, 5);
        cycles = 8;
        break;
    case 0xAA: // RES 5, D
        RESET_BIT(cpu.registers.d, 5);
        cycles = 8;
        break;
    case 0xAB: // RES 5, E
        RESET_BIT(cpu.registers.e, 5);
        cycles = 8;
        break;
    case 0xAC: // RES 5, H
        RESET_BIT(cpu.registers.h, 5);
        cycles = 8;
        break;
    case 0xAD: // RES 5, L
        RESET_BIT(cpu.registers.l, 5);
        cycles = 8;
        break;
    case 0xAE: // RES 5, (HL)
        temp = mmu_read(cpu.registers.hl);
        mmu_write(cpu.registers.hl, RESET_BIT(temp, 5));
        cycles = 16;
        break;
    case 0xAF: // RES 5, A
        RESET_BIT(cpu.registers.a, 5);
        cycles = 8;
        break;
    case 0xB0: // RES 6, B
        RESET_BIT(cpu.registers.b, 6);
        cycles = 8;
        break;
    case 0xB1: // RES 6, C
        RESET_BIT(cpu.registers.c, 6);
        cycles = 8;
        break;
    case 0xB2: // RES 6, D
        RESET_BIT(cpu.registers.d, 6);
        cycles = 8;
        break;
    case 0xB3: // RES 6, E
        RESET_BIT(cpu.registers.e, 6);
        cycles = 8;
        break;
    case 0xB4: // RES 6, H
        RESET_BIT(cpu.registers.h, 6);
        cycles = 8;
        break;
    case 0xB5: // RES 6, L
        RESET_BIT(cpu.registers.l, 6);
        cycles = 8;
        break;
    case 0xB6: // RES 6, (HL)
        temp = mmu_read(cpu.registers.hl);
        mmu_write(cpu.registers.hl, RESET_BIT(temp, 6));
        cycles = 16;
        break;
    case 0xB7: // RES 6, A
        RESET_BIT(cpu.registers.a, 6);
        cycles = 8;
        break;
    case 0xB8: // RES 7, B
        RESET_BIT(cpu.registers.b, 7);
        cycles = 8;
        break;
    case 0xB9: // RES 7, C
        RESET_BIT(cpu.registers.c, 7);
        cycles = 8;
        break;
    case 0xBA: // RES 7, D
        RESET_BIT(cpu.registers.d, 7);
        cycles = 8;
        break;
    case 0xBB: // RES 7, E
        RESET_BIT(cpu.registers.e, 7);
        cycles = 8;
        break;
    case 0xBC: // RES 7, H
        RESET_BIT(cpu.registers.h, 7);
        cycles = 8;
        break;
    case 0xBD: // RES 7, L
        RESET_BIT(cpu.registers.l, 7);
        cycles = 8;
        break;
    case 0xBE: // RES 7, (HL)
        temp = mmu_read(cpu.registers.hl);
        mmu_write(cpu.registers.hl, RESET_BIT(temp, 7));
        cycles = 16;
        break;
    case 0xBF: // RES 7, A
        RESET_BIT(cpu.registers.a, 7);
        cycles = 8;
        break;
    case 0xC0: // SET 0, B
        SET_BIT(cpu.registers.b, 0);
        cycles = 8;
        break;
    case 0xC1: // SET 0, C
        SET_BIT(cpu.registers.c, 0);
        cycles = 8;
        break;
    case 0xC2: // SET 0, D
        SET_BIT(cpu.registers.d, 0);
        cycles = 8;
        break;
    case 0xC3: // SET 0, E
        SET_BIT(cpu.registers.e, 0);
        cycles = 8;
        break;
    case 0xC4: // SET 0, H
        SET_BIT(cpu.registers.h, 0);
        cycles = 8;
        break;
    case 0xC5: // SET 0, L
        SET_BIT(cpu.registers.l, 0);
        cycles = 8;
        break;
    case 0xC6: // SET 0, (HL)
        temp = mmu_read(cpu.registers.hl);
        mmu_write(cpu.registers.hl, SET_BIT(temp, 0));
        cycles = 16;
        break;
    case 0xC7: // SET 0, A
        SET_BIT(cpu.registers.a, 0);
        cycles = 8;
        break;
    case 0xC8: // SET 1, B
        SET_BIT(cpu.registers.b, 1);
        cycles = 8;
        break;
    case 0xC9: // SET 1, C
        SET_BIT(cpu.registers.c, 1);
        cycles = 8;
        break;
    case 0xCA: // SET 1, D
        SET_BIT(cpu.registers.d, 1);
        cycles = 8;
        break;
    case 0xCB: // SET 1, E
        SET_BIT(cpu.registers.e, 1);
        cycles = 8;
        break;
    case 0xCC: // SET 1, H
        SET_BIT(cpu.registers.h, 1);
        cycles = 8;
        break;
    case 0xCD: // SET 1, L
        SET_BIT(cpu.registers.l, 1);
        cycles = 8;
        break;
    case 0xCE: // SET 1, (HL)
        temp = mmu_read(cpu.registers.hl);
        mmu_write(cpu.registers.hl, SET_BIT(temp, 1));
        cycles = 16;
        break;
    case 0xCF: // SET 1, A
        SET_BIT(cpu.registers.a, 1);
        cycles = 8;
        break;
    case 0xD0: // SET 2, B
        SET_BIT(cpu.registers.b, 2);
        cycles = 8;
        break;
    case 0xD1: // SET 2, C
        SET_BIT(cpu.registers.c, 2);
        cycles = 8;
        break;
    case 0xD2: // SET 2, D
        SET_BIT(cpu.registers.d, 2);
        cycles = 8;
        break;
    case 0xD3: // SET 2, E
        SET_BIT(cpu.registers.e, 2);
        cycles = 8;
        break;
    case 0xD4: // SET 2, H
        SET_BIT(cpu.registers.h, 2);
        cycles = 8;
        break;
    case 0xD5: // SET 2, L
        SET_BIT(cpu.registers.l, 2);
        cycles = 8;
        break;
    case 0xD6: // SET 2, (HL)
        temp = mmu_read(cpu.registers.hl);
        mmu_write(cpu.registers.hl, SET_BIT(temp, 2));
        cycles = 16;
        break;
    case 0xD7: // SET 2, A
        SET_BIT(cpu.registers.a, 2);
        cycles = 8;
        break;
    case 0xD8: // SET 3, B
        SET_BIT(cpu.registers.b, 3);
        cycles = 8;
        break;
    case 0xD9: // SET 3, C
        SET_BIT(cpu.registers.c, 3);
        cycles = 8;
        break;
    case 0xDA: // SET 3, D
        SET_BIT(cpu.registers.d, 3);
        cycles = 8;
        break;
    case 0xDB: // SET 3, E
        SET_BIT(cpu.registers.e, 3);
        cycles = 8;
        break;
    case 0xDC: // SET 3, H
        SET_BIT(cpu.registers.h, 3);
        cycles = 8;
        break;
    case 0xDD: // SET 3, L
        SET_BIT(cpu.registers.l, 3);
        cycles = 8;
        break;
    case 0xDE: // SET 3, (HL)
        temp = mmu_read(cpu.registers.hl);
        mmu_write(cpu.registers.hl, SET_BIT(temp, 3));
        cycles = 16;
        break;
    case 0xDF: // SET 3, A
        SET_BIT(cpu.registers.a, 3);
        cycles = 8;
        break;
    case 0xE0: // SET 4, B
        SET_BIT(cpu.registers.b, 4);
        cycles = 8;
        break;
    case 0xE1: // SET 4, C
        SET_BIT(cpu.registers.c, 4);
        cycles = 8;
        break;
    case 0xE2: // SET 4, D
        SET_BIT(cpu.registers.d, 4);
        cycles = 8;
        break;
    case 0xE3: // SET 4, E
        SET_BIT(cpu.registers.e, 4);
        cycles = 8;
        break;
    case 0xE4: // SET 4, H
        SET_BIT(cpu.registers.h, 4);
        cycles = 8;
        break;
    case 0xE5: // SET 4, L
        SET_BIT(cpu.registers.l, 4);
        cycles = 8;
        break;
    case 0xE6: // SET 4, (HL)
        temp = mmu_read(cpu.registers.hl);
        mmu_write(cpu.registers.hl, SET_BIT(temp, 4));
        cycles = 16;
        break;
    case 0xE7: // SET 4, A
        SET_BIT(cpu.registers.a, 4);
        cycles = 8;
        break;
    case 0xE8: // SET 5, B
        SET_BIT(cpu.registers.b, 5);
        cycles = 8;
        break;
    case 0xE9: // SET 5, C
        SET_BIT(cpu.registers.c, 5);
        cycles = 8;
        break;
    case 0xEA: // SET 5, D
        SET_BIT(cpu.registers.d, 5);
        cycles = 8;
        break;
    case 0xEB: // SET 5, E
        SET_BIT(cpu.registers.e, 5);
        cycles = 8;
        break;
    case 0xEC: // SET 5, H
        SET_BIT(cpu.registers.h, 5);
        cycles = 8;
        break;
    case 0xED: // SET 5, L
        SET_BIT(cpu.registers.l, 5);
        cycles = 8;
        break;
    case 0xEE: // SET 5, (HL)
        temp = mmu_read(cpu.registers.hl);
        mmu_write(cpu.registers.hl, SET_BIT(temp, 5));
        cycles = 16;
        break;
    case 0xEF: // SET 5, A
        SET_BIT(cpu.registers.a, 5);
        cycles = 8;
        break;
    case 0xF0: // SET 6, B
        SET_BIT(cpu.registers.b, 6);
        cycles = 8;
        break;
    case 0xF1: // SET 6, C
        SET_BIT(cpu.registers.c, 6);
        cycles = 8;
        break;
    case 0xF2: // SET 6, D
        SET_BIT(cpu.registers.d, 6);
        cycles = 8;
        break;
    case 0xF3: // SET 6, E
        SET_BIT(cpu.registers.e, 6);
        cycles = 8;
        break;
    case 0xF4: // SET 6, H
        SET_BIT(cpu.registers.h, 6);
        cycles = 8;
        break;
    case 0xF5: // SET 6, L
        SET_BIT(cpu.registers.l, 6);
        cycles = 8;
        break;
    case 0xF6: // SET 6, (HL)
        temp = mmu_read(cpu.registers.hl);
        mmu_write(cpu.registers.hl, SET_BIT(temp, 6));
        cycles = 16;
        break;
    case 0xF7: // SET 6, A
        SET_BIT(cpu.registers.a, 6);
        cycles = 8;
        break;
    case 0xF8: // SET 7, B
        SET_BIT(cpu.registers.b, 7);
        cycles = 8;
        break;
    case 0xF9: // SET 7, C
        SET_BIT(cpu.registers.c, 7);
        cycles = 8;
        break;
    case 0xFA: // SET 7, D
        SET_BIT(cpu.registers.d, 7);
        cycles = 8;
        break;
    case 0xFB: // SET 7, E
        SET_BIT(cpu.registers.e, 7);
        cycles = 8;
        break;
    case 0xFC: // SET 7, H
        SET_BIT(cpu.registers.h, 7);
        cycles = 8;
        break;
    case 0xFD: // SET 7, L
        SET_BIT(cpu.registers.l, 7);
        cycles = 8;
        break;
    case 0xFE: // SET 7, (HL)
        temp = mmu_read(cpu.registers.hl);
        mmu_write(cpu.registers.hl, SET_BIT(temp, 7));
        cycles = 16;
        break;
    case 0xFF: // SET 7, A
        SET_BIT(cpu.registers.a, 7);
        cycles = 8;
        break;
    default:
        printf("ERROR: opcode CB %02X: %s\n", opcode, extended_instructions[opcode].name);
        exit(EXIT_FAILURE);
        break;
    }

    return cycles;
}

static int exec_opcode(byte_t opcode, word_t operand) {
    int cycles;
    unsigned int result; // temporary variable used for some opcodes

    switch (opcode) {
    case 0x00: // NOP
        cycles = 4;
        break;
    case 0x01: // LD BC, nn
        cpu.registers.bc = operand;
        cycles = 12;
        break;
    case 0x02: // LD (BC),A
        mmu_write(cpu.registers.bc, cpu.registers.a);
        cycles = 8;
        break;
    case 0x03: // INC BC
        cpu.registers.bc++;
        cycles = 8;
        break;
    case 0x04: // INC B
        cpu.registers.b = inc(cpu.registers.b);
        cycles = 4;
        break;
    case 0x05: // DEC B
        cpu.registers.b = dec(cpu.registers.b);
        cycles = 4;
        break;
    case 0x06: // LD B, n
        cpu.registers.b = operand;
        cycles = 8;
        break;
    case 0x07: // RLCA
        cpu.registers.a = rlc(cpu.registers.a);
        RESET_FLAG(FLAG_Z);
        cycles = 4;
        break;
    case 0x08: // LD (nn), SP
        mmu_write(operand, cpu.registers.sp & 0xFF);
        mmu_write(operand + 1, cpu.registers.sp >> 8);
        cycles = 20;
        break;
    case 0x09: // ADD HL, BC
        add16(cpu.registers.bc);
        cycles = 8;
        break;
    case 0x0A: // LD A,(BC)
        cpu.registers.a = mmu_read(cpu.registers.bc);
        cycles = 8;
        break;
    case 0x0B: // DEC BC
        cpu.registers.bc--;
        cycles = 8;
        break;
    case 0x0C: // INC C
        cpu.registers.c = inc(cpu.registers.c);
        cycles = 4;
        break;
    case 0x0D: // DEC C
        cpu.registers.c = dec(cpu.registers.c);
        cycles = 4;
        break;
    case 0x0E: // LD C, n
        cpu.registers.c = operand;
        cycles = 8;
        break;
    case 0x0F: // RRCA
        cpu.registers.a = rrc(cpu.registers.a);
        RESET_FLAG(FLAG_Z);
        cycles = 4;
        break;
    case 0x10: // STOP
        // TODO Halts until button press. Blargg's cpu_instrs.gb test rom wrongly assumes this is a CGB emulator and will reach this opcode. 
        cycles = 4;
        break;
    case 0x11: // LD DE, nn
        cpu.registers.de = operand;
        cycles = 12;
        break;
    case 0x12: // LD (DE),A
        mmu_write(cpu.registers.de, cpu.registers.a);
        cycles = 8;
        break;
    case 0x13: // INC DE
        cpu.registers.de++;
        cycles = 8;
        break;
    case 0x14: // INC D
        cpu.registers.d = inc(cpu.registers.d);
        cycles = 4;
        break;
    case 0x15: // DEC D
        cpu.registers.d = dec(cpu.registers.d);
        cycles = 4;
        break;
    case 0x16: // LD D, n
        cpu.registers.d = operand;
        cycles = 8;
        break;
    case 0x17: // RLA
        cpu.registers.a = rl(cpu.registers.a);
        RESET_FLAG(FLAG_Z);
        cycles = 4;
        break;
    case 0x18: // JR n
        cpu.registers.pc += (s_byte_t) operand;
        cycles = 12;
        break;
    case 0x19: // ADD HL, DE
        add16(cpu.registers.de);
        cycles = 8;
        break;
    case 0x1A: // LD A,(DE)
        cpu.registers.a = mmu_read(cpu.registers.de);
        cycles = 8;
        break;
    case 0x1B: // DEC DE
        cpu.registers.de--;
        cycles = 8;
        break;
    case 0x1C: // INC E
        cpu.registers.e = inc(cpu.registers.e);
        cycles = 4;
        break;
    case 0x1D: // DEC E
        cpu.registers.e = dec(cpu.registers.e);
        cycles = 4;
        break;
    case 0x1E: // LD E, n
        cpu.registers.e = operand;
        cycles = 8;
        break;
    case 0x1F: // RRA
        cpu.registers.a = rr(cpu.registers.a);
        RESET_FLAG(FLAG_Z);
        cycles = 4;
        break;
    case 0x20: // JR NZ, n
        if (!CHECK_FLAG(FLAG_Z)) {
            cpu.registers.pc += (s_byte_t) operand;
            cycles = 12;
        } else {
            cycles = 8;
        }
        break;
    case 0x21: // LD HL, nn
        cpu.registers.hl = operand;
        cycles = 12;
        break;
    case 0x22: // LDI (HL), A
        mmu_write(cpu.registers.hl, cpu.registers.a);
        cpu.registers.hl++;
        cycles = 8;
        break;
    case 0x23: // INC HL
        cpu.registers.hl++;
        cycles = 8;
        break;
    case 0x24: // INC H
        cpu.registers.h = inc(cpu.registers.h);
        cycles = 4;
        break;
    case 0x25: // DEC H
        cpu.registers.h = dec(cpu.registers.h);
        cycles = 4;
        break;
    case 0x26: // LD H, n
        cpu.registers.h = operand;
        cycles = 8;
        break;
    case 0x27: // DAA
        if (!CHECK_FLAG(FLAG_N)) {
            if (CHECK_FLAG(FLAG_C) || cpu.registers.a > 0x99) {
                cpu.registers.a += 0x60;
                SET_FLAG(FLAG_C);
            }
            if (CHECK_FLAG(FLAG_H) || (cpu.registers.a & 0x0f) > 0x09)
                cpu.registers.a += 0x06;
        } else {
            if (CHECK_FLAG(FLAG_C)) {
                cpu.registers.a -= 0x60;
                SET_FLAG(FLAG_C);
            }
            if (CHECK_FLAG(FLAG_H))
                cpu.registers.a -= 0x06;
        }
        cpu.registers.a ? RESET_FLAG(FLAG_Z) : SET_FLAG(FLAG_Z);
        RESET_FLAG(FLAG_H);
        cycles = 4;
        break;
    case 0x28: // JR Z, n
        if (CHECK_FLAG(FLAG_Z)) {
            cpu.registers.pc += (s_byte_t) operand;
            cycles = 12;
        } else {
            cycles = 8;
        }
        break;
    case 0x29: // ADD HL, HL
        add16(cpu.registers.hl);
        cycles = 8;
        break;
    case 0x2A: // LDI A, (HL)
        cpu.registers.a = mmu_read(cpu.registers.hl);
        cpu.registers.hl++;
        cycles = 8;
        break;
    case 0x2B: // DEC HL
        cpu.registers.hl--;
        cycles = 8;
        break;
    case 0x2C: // INC L
        cpu.registers.l = inc(cpu.registers.l);
        cycles = 4;
        break;
    case 0x2D: // DEC L
        cpu.registers.l = dec(cpu.registers.l);
        cycles = 4;
        break;
    case 0x2E: // LD L, n
        cpu.registers.l = operand;
        cycles = 8;
        break;
    case 0x2F: // CPL
        cpu.registers.a = ~cpu.registers.a;
        SET_FLAG(FLAG_N | FLAG_H);
        cycles = 4;
        break;
    case 0x30: // JR NC, n
        if (!CHECK_FLAG(FLAG_C)) {
            cpu.registers.pc += (s_byte_t) operand;
            cycles = 12;
        } else {
            cycles = 8;
        }
        break;
    case 0x31: // LD SP,nn
        cpu.registers.sp = operand;
        cycles = 12;
        break;
    case 0x32: // LDD (HL),A
        mmu_write(cpu.registers.hl, cpu.registers.a);
        cpu.registers.hl--;
        cycles = 8;
        break;
    case 0x33: // INC SP
        cpu.registers.sp++;
        cycles = 8;
        break;
    case 0x34: // INC (HL)
        mmu_write(cpu.registers.hl, inc(mmu_read(cpu.registers.hl)));
        cycles = 12;
        break;
    case 0x35: // DEC (HL)
        mmu_write(cpu.registers.hl, dec(mmu_read(cpu.registers.hl)));
        cycles = 12;
        break;
    case 0x36: // LD (HL),n
        mmu_write(cpu.registers.hl, operand);
        cycles = 12;
        break;
    case 0x37: // SCF
        RESET_FLAG(FLAG_N | FLAG_H);
        SET_FLAG(FLAG_C);
        cycles = 4;
        break;
    case 0x38: // JR C, n
        if (CHECK_FLAG(FLAG_C)) {
            cpu.registers.pc += (s_byte_t) operand;
            cycles = 12;
        } else {
            cycles = 8;
        }
        break;
    case 0x39: // ADD HL, SP
        add16(cpu.registers.sp);
        cycles = 8;
        break;
    case 0x3A: // LDD A, (HL)
        cpu.registers.a = mmu_read(cpu.registers.hl);
        cpu.registers.hl--;
        cycles = 8;
        break;
    case 0x3B: // DEC SP
        cpu.registers.sp--;
        cycles = 8;
        break;
    case 0x3C: // INC A
        cpu.registers.a = inc(cpu.registers.a);
        cycles = 4;
        break;
    case 0x3D: // DEC A
        cpu.registers.a = dec(cpu.registers.a);
        cycles = 4;
        break;
    case 0x3E: // LD A,n
        cpu.registers.a = operand;
        cycles = 8;
        break;
    case 0x3F: // CCF
        CHECK_FLAG(FLAG_C) ? RESET_FLAG(FLAG_C) : SET_FLAG(FLAG_C);
        RESET_FLAG(FLAG_N | FLAG_H);
        cycles = 4;
        break;
    case 0x40: // LD B,B
        // cpu.registers.b = cpu.registers.b;
        cycles = 4;
        break;
    case 0x41: // LD B,C
        cpu.registers.b = cpu.registers.c;
        cycles = 4;
        break;
    case 0x42: // LD B,D
        cpu.registers.b = cpu.registers.d;
        cycles = 4;
        break;
    case 0x43: // LD B,E
        cpu.registers.b = cpu.registers.e;
        cycles = 4;
        break;
    case 0x44: // LD B,H
        cpu.registers.b = cpu.registers.h;
        cycles = 4;
        break;
    case 0x45: // LD B,L
        cpu.registers.b = cpu.registers.l;
        cycles = 4;
        break;
    case 0x46: // LD B,(HL)
        cpu.registers.b = mmu_read(cpu.registers.hl);
        cycles = 8;
        break;
    case 0x47: // LD B,A
        cpu.registers.b = cpu.registers.a;
        cycles = 4;
        break;
    case 0x48: // LD C,B
        cpu.registers.c = cpu.registers.b;
        cycles = 4;
        break;
    case 0x49: // LD C,C
        // cpu.registers.c = cpu.registers.c;
        cycles = 4;
        break;
    case 0x4A: // LD C,D
        cpu.registers.c = cpu.registers.d;
        cycles = 4;
        break;
    case 0x4B: // LD C,E
        cpu.registers.c = cpu.registers.e;
        cycles = 4;
        break;
    case 0x4C: // LD C,H
        cpu.registers.c = cpu.registers.h;
        cycles = 4;
        break;
    case 0x4D: // LD C,L
        cpu.registers.c = cpu.registers.l;
        cycles = 4;
        break;
    case 0x4E: // LD C,(HL)
        cpu.registers.c = mmu_read(cpu.registers.hl);
        cycles = 8;
        break;
    case 0x4F: // LD C,A
        cpu.registers.c = cpu.registers.a;
        cycles = 4;
        break;
    case 0x50: // LD D,B
        cpu.registers.d = cpu.registers.b;
        cycles = 4;
        break;
    case 0x51: // LD D,C
        cpu.registers.d = cpu.registers.c;
        cycles = 4;
        break;
    case 0x52: // LD D,D
        // cpu.registers.d = cpu.registers.d;
        cycles = 4;
        break;
    case 0x53: // LD D,E
        cpu.registers.d = cpu.registers.e;
        cycles = 4;
        break;
    case 0x54: // LD D,H
        cpu.registers.d = cpu.registers.h;
        cycles = 4;
        break;
    case 0x55: // LD D,L
        cpu.registers.d = cpu.registers.l;
        cycles = 4;
        break;
    case 0x56: // LD D,(HL)
        cpu.registers.d = mmu_read(cpu.registers.hl);
        cycles = 8;
        break;
    case 0x57: // LD D,A
        cpu.registers.d = cpu.registers.a;
        cycles = 4;
        break;
    case 0x58: // LD E,B
        cpu.registers.e = cpu.registers.b;
        cycles = 4;
        break;
    case 0x59: // LD E,C
        cpu.registers.e = cpu.registers.c;
        cycles = 4;
        break;
    case 0x5A: // LD E,D
        cpu.registers.e = cpu.registers.d;
        cycles = 4;
        break;
    case 0x5B: // LD E,E
        // cpu.registers.e = cpu.registers.e;
        cycles = 4;
        break;
    case 0x5C: // LD E,H
        cpu.registers.e = cpu.registers.h;
        cycles = 4;
        break;
    case 0x5D: // LD E,L
        cpu.registers.e = cpu.registers.l;
        cycles = 4;
        break;
    case 0x5E: // LD E,(HL)
        cpu.registers.e = mmu_read(cpu.registers.hl);
        cycles = 8;
        break;
    case 0x5F: // LD E,A
        cpu.registers.e = cpu.registers.a;
        cycles = 4;
        break;
    case 0x60: // LD H,B
        cpu.registers.h = cpu.registers.b;
        cycles = 4;
        break;
    case 0x61: // LD H,C
        cpu.registers.h = cpu.registers.c;
        cycles = 4;
        break;
    case 0x62: // LD H,D
        cpu.registers.h = cpu.registers.d;
        cycles = 4;
        break;
    case 0x63: // LD H,E
        cpu.registers.h = cpu.registers.e;
        cycles = 4;
        break;
    case 0x64: // LD H,H
        // cpu.registers.h = cpu.registers.h;
        cycles = 4;
        break;
    case 0x65: // LD H,L
        cpu.registers.h = cpu.registers.l;
        cycles = 4;
        break;
    case 0x66: // LD H,(HL)
        cpu.registers.h = mmu_read(cpu.registers.hl);
        cycles = 8;
        break;
    case 0x67: // LD H,A
        cpu.registers.h = cpu.registers.a;
        cycles = 4;
        break;
    case 0x68: // LD L,B
        cpu.registers.l = cpu.registers.b;
        cycles = 4;
        break;
    case 0x69: // LD L,C
        cpu.registers.l = cpu.registers.c;
        cycles = 4;
        break;
    case 0x6A: // LD L,D
        cpu.registers.l = cpu.registers.d;
        cycles = 4;
        break;
    case 0x6B: // LD L,E
        cpu.registers.l = cpu.registers.e;
        cycles = 4;
        break;
    case 0x6C: // LD L,H
        cpu.registers.l = cpu.registers.h;
        cycles = 4;
        break;
    case 0x6D: // LD L,L
        // cpu.registers.l = cpu.registers.l;
        cycles = 4;
        break;
    case 0x6E: // LD L,(HL)
        cpu.registers.l = mmu_read(cpu.registers.hl);
        cycles = 8;
        break;
    case 0x6F: // LD L,A
        cpu.registers.l = cpu.registers.a;
        cycles = 4;
        break;
    case 0x70: // LD (HL),B
        mmu_write(cpu.registers.hl, cpu.registers.b);
        cycles = 8;
        break;
    case 0x71: // LD (HL),C
        mmu_write(cpu.registers.hl, cpu.registers.c);
        cycles = 8;
        break;
    case 0x72: // LD (HL),D
        mmu_write(cpu.registers.hl, cpu.registers.d);
        cycles = 8;
        break;
    case 0x73: // LD (HL),E
        mmu_write(cpu.registers.hl, cpu.registers.e);
        cycles = 8;
        break;
    case 0x74: // LD (HL),H
        mmu_write(cpu.registers.hl, cpu.registers.h);
        cycles = 8;
        break;
    case 0x75: // LD (HL),L
        mmu_write(cpu.registers.hl, cpu.registers.l);
        cycles = 8;
        break;
    case 0x76: // HALT
        cpu.halt = 1;
        cycles = 4;
        break;
    case 0x77: // LD (HL),A
        mmu_write(cpu.registers.hl, cpu.registers.a);
        cycles = 8;
        break;
    case 0x78: // LD A,B
        cpu.registers.a = cpu.registers.b;
        cycles = 4;
        break;
    case 0x79: // LD A,C
        cpu.registers.a = cpu.registers.c;
        cycles = 4;
        break;
    case 0x7A: // LD A,D
        cpu.registers.a = cpu.registers.d;
        cycles = 4;
        break;
    case 0x7B: // LD A,E
        cpu.registers.a = cpu.registers.e;
        cycles = 4;
        break;
    case 0x7C: // LD A,H
        cpu.registers.a = cpu.registers.h;
        cycles = 4;
        break;
    case 0x7D: // LD A,L
        cpu.registers.a = cpu.registers.l;
        cycles = 4;
        break;
    case 0x7E: // LD A,(HL)
        cpu.registers.a = mmu_read(cpu.registers.hl);
        cycles = 8;
        break;
    case 0x7F: // LD A,A
        // cpu.registers.a = cpu.registers.a;
        cycles = 4;
        break;
    case 0x80: // ADD A, B
        add8(cpu.registers.b);
        cycles = 4;
        break;
    case 0x81: // ADD A, C
        add8(cpu.registers.c);
        cycles = 4;
        break;
    case 0x82: // ADD A, D
        add8(cpu.registers.d);
        cycles = 4;
        break;
    case 0x83: // ADD A, E
        add8(cpu.registers.e);
        cycles = 4;
        break;
    case 0x84: // ADD A, H
        add8(cpu.registers.h);
        cycles = 4;
        break;
    case 0x85: // ADD A, L
        add8(cpu.registers.l);
        cycles = 4;
        break;
    case 0x86: // ADD A, (HL)
        add8(mmu_read(cpu.registers.hl));
        cycles = 8;
        break;
    case 0x87: // ADD A, A
        add8(cpu.registers.a);
        cycles = 4;
        break;
    case 0x88: // ADC A, B
        adc(cpu.registers.b);
        cycles = 4;
        break;
    case 0x89: // ADC A, C
        adc(cpu.registers.c);
        cycles = 4;
        break;
    case 0x8A: // ADC A, D
        adc(cpu.registers.d);
        cycles = 4;
        break;
    case 0x8B: // ADC A, E
        adc(cpu.registers.e);
        cycles = 4;
        break;
    case 0x8C: // ADC A, H
        adc(cpu.registers.h);
        cycles = 4;
        break;
    case 0x8D: // ADC A, L
        adc(cpu.registers.l);
        cycles = 4;
        break;
    case 0x8E: // ADC A, (HL)
        adc(mmu_read(cpu.registers.hl));
        cycles = 8;
        break;
    case 0x8F: // ADC A, A
        adc(cpu.registers.a);
        cycles = 4;
        break;
    case 0x90: // SUB A, B
        sub8(cpu.registers.b);
        cycles = 4;
        break;
    case 0x91: // SUB A, C
        sub8(cpu.registers.c);
        cycles = 4;
        break;
    case 0x92: // SUB A, D
        sub8(cpu.registers.d);
        cycles = 4;
        break;
    case 0x93: // SUB A, E
        sub8(cpu.registers.e);
        cycles = 4;
        break;
    case 0x94: // SUB A, H
        sub8(cpu.registers.h);
        cycles = 4;
        break;
    case 0x95: // SUB A, L
        sub8(cpu.registers.l);
        cycles = 4;
        break;
    case 0x96: // SUB A, (HL)
        sub8(mmu_read(cpu.registers.hl));
        cycles = 8;
        break;
    case 0x97: // SUB A, A
        sub8(cpu.registers.a);
        cycles = 4;
        break;
    case 0x98: // SBC A, B
        sbc(cpu.registers.b);
        cycles = 4;
        break;
    case 0x99: // SBC A, C
        sbc(cpu.registers.c);
        cycles = 4;
        break;
    case 0x9A: // SBC A, D
        sbc(cpu.registers.d);
        cycles = 4;
        break;
    case 0x9B: // SBC A, E
        sbc(cpu.registers.e);
        cycles = 4;
        break;
    case 0x9C: // SBC A, H
        sbc(cpu.registers.h);
        cycles = 4;
        break;
    case 0x9D: // SBC A, L
        sbc(cpu.registers.l);
        cycles = 4;
        break;
    case 0x9E: // SBC A, (HL)
        sbc(mmu_read(cpu.registers.hl));
        cycles = 8;
        break;
    case 0x9F: // SBC A, A
        sbc(cpu.registers.a);
        cycles = 4;
        break;
    case 0xA0: // AND B
        and(cpu.registers.b);
        cycles = 4;
        break;
    case 0xA1: // AND C
        and(cpu.registers.c);
        cycles = 4;
        break;
    case 0xA2: // AND D
        and(cpu.registers.d);
        cycles = 4;
        break;
    case 0xA3: // AND E
        and(cpu.registers.e);
        cycles = 4;
        break;
    case 0xA4: // AND H
        and(cpu.registers.h);
        cycles = 4;
        break;
    case 0xA5: // AND L
        and(cpu.registers.l);
        cycles = 4;
        break;
    case 0xA6: // AND (HL)
        and(mmu_read(cpu.registers.hl));
        cycles = 8;
        break;
    case 0xA7: // AND A
        and(cpu.registers.a);
        cycles = 4;
        break;
    case 0xA8: // XOR B
        xor(cpu.registers.b);
        cycles = 4;
        break;
    case 0xA9: // XOR C
        xor(cpu.registers.c);
        cycles = 4;
        break;
    case 0xAA: // XOR D
        xor(cpu.registers.d);
        cycles = 4;
        break;
    case 0xAB: // XOR E
        xor(cpu.registers.e);
        cycles = 4;
        break;
    case 0xAC: // XOR H
        xor(cpu.registers.h);
        cycles = 4;
        break;
    case 0xAD: // XOR L
        xor(cpu.registers.l);
        cycles = 4;
        break;
    case 0xAE: // XOR (HL)
        xor(mmu_read(cpu.registers.hl));
        cycles = 8;
        break;
    case 0xAF: // XOR A
        xor(cpu.registers.a);
        cycles = 4;
        break;
    case 0xB0: // OR B
        or(cpu.registers.b);
        cycles = 4;
        break;
    case 0xB1: // OR C
        or(cpu.registers.c);
        cycles = 4;
        break;
    case 0xB2: // OR D
        or(cpu.registers.d);
        cycles = 4;
        break;
    case 0xB3: // OR E
        or(cpu.registers.e);
        cycles = 4;
        break;
    case 0xB4: // OR H
        or(cpu.registers.h);
        cycles = 4;
        break;
    case 0xB5: // OR L
        or(cpu.registers.l);
        cycles = 4;
        break;
    case 0xB6: // OR (HL)
        or(mmu_read(cpu.registers.hl));
        cycles = 8;
        break;
    case 0xB7: // OR A
        or(cpu.registers.a);
        cycles = 4;
        break;
    case 0xB8: // CP B
        cp(cpu.registers.b);
        cycles = 4;
        break;
    case 0xB9: // CP C
        cp(cpu.registers.c);
        cycles = 4;
        break;
    case 0xBA: // CP D
        cp(cpu.registers.d);
        cycles = 4;
        break;
    case 0xBB: // CP E
        cp(cpu.registers.e);
        cycles = 4;
        break;
    case 0xBC: // CP H
        cp(cpu.registers.h);
        cycles = 4;
        break;
    case 0xBD: // CP L
        cp(cpu.registers.l);
        cycles = 4;
        break;
    case 0xBE: // CP (HL)
        cp(mmu_read(cpu.registers.hl));
        cycles = 8;
        break;
    case 0xBF: // CP A
        cp(cpu.registers.a);
        cycles = 4;
        break;
    case 0xC0: // RET NZ
        if (!CHECK_FLAG(FLAG_Z)) {
            cpu.registers.pc = pop();
            cycles = 20;
        } else {
            cycles = 8;
        }
        break;
    case 0xC1: // POP BC
        cpu.registers.bc = pop();
        cycles = 12;
        break;
    case 0xC2: // JP NZ, nn
        if (!CHECK_FLAG(FLAG_Z)) {
            cpu.registers.pc = operand;
            cycles = 16;
        } else {
            cycles = 12;
        }
        break;
    case 0xC3: // JP nn
        cpu.registers.pc = operand;
        cycles = 16;
        break;
    case 0xC4: // CALL NZ, nn
        if (!CHECK_FLAG(FLAG_Z)) {
            push(cpu.registers.pc);
            cpu.registers.pc = operand;
            cycles = 24;
        } else {
            cycles = 12;
        }
        break;
    case 0xC5: // PUSH BC
        push(cpu.registers.bc);
        cycles = 16;
        break;
    case 0xC6: // ADD A, n
        add8(operand);
        cycles = 8;
        break;
    case 0xC7: // RST 0x00
        push(cpu.registers.pc);
        cpu.registers.pc = 0x0000;
        cycles = 16;
        break;
    case 0xC8: // RET Z
        if (CHECK_FLAG(FLAG_Z)) {
            cpu.registers.pc = pop();
            cycles = 20;
        } else {
            cycles = 8;
        }
        break;
    case 0xC9: // RET
        cpu.registers.pc = pop();
        cycles = 16;
        break;
    case 0xCA: // JP Z, nn
        if (CHECK_FLAG(FLAG_Z)) {
            cpu.registers.pc = operand;
            cycles = 16;
        } else {
            cycles = 12;
        }
        break;
    case 0xCB: // CB nn (prefix instruction)
        return exec_extended_opcode(operand);
    case 0xCC: // CALL Z, nn
        if (CHECK_FLAG(FLAG_Z)) {
            push(cpu.registers.pc);
            cpu.registers.pc = operand;
            cycles = 24;
        } else {
            cycles = 12;
        }
        break;
    case 0xCD: // CALL nn
        push(cpu.registers.pc);
        cpu.registers.pc = operand;
        cycles = 24;
        break;
    case 0xCE: // ADC A, n
        adc(operand);
        cycles = 8;
        break;
    case 0xCF: // RST 0x08
        push(cpu.registers.pc);
        cpu.registers.pc = 0x0008;
        cycles = 16;
        break;
    case 0xD0: // RET NC
        if (!CHECK_FLAG(FLAG_C)) {
            cpu.registers.pc = pop();
            cycles = 20;
        } else {
            cycles = 8;
        }
        break;
    case 0xD1: // POP DE
        cpu.registers.de = pop();
        cycles = 12;
        break;
    case 0xD2: // JP NC, nn
        if (!CHECK_FLAG(FLAG_C)) {
            cpu.registers.pc = operand;
            cycles = 16;
        } else {
            cycles = 12;
        }
        break;
    case 0xD4: // CALL NC, nn
        if (!CHECK_FLAG(FLAG_C)) {
            push(cpu.registers.pc);
            cpu.registers.pc = operand;
            cycles = 24;
        } else {
            cycles = 12;
        }
        break;
    case 0xD5: // PUSH DE
        push(cpu.registers.de);
        cycles = 16;
        break;
    case 0xD6: // SUB A, n
        sub8(operand);
        cycles = 8;
        break;
    case 0xD7: // RST 0x10
        push(cpu.registers.pc);
        cpu.registers.pc = 0x0010;
        cycles = 16;
        break;
    case 0xD8: // RET C
        if (CHECK_FLAG(FLAG_C)) {
            cpu.registers.pc = pop();
            cycles = 20;
        } else {
            cycles = 8;
        }
        break;
    case 0xD9: // RETI
        cpu.registers.pc = pop();
        cpu.ime = 1;
        cycles = 16;
        break;
    case 0xDA: // JP C, nn
        if (CHECK_FLAG(FLAG_C)) {
            cpu.registers.pc = operand;
            cycles = 16;
        } else {
            cycles = 12;
        }
        break;
    case 0xDC: // CALL C, nn
        if (CHECK_FLAG(FLAG_C)) {
            push(cpu.registers.pc);
            cpu.registers.pc = operand;
            cycles = 24;
        } else {
            cycles = 12;
        }
        break;
    case 0xDE: // SBC A, n
        sbc(operand);
        cycles = 8;
        break;
    case 0xDF: // RST 0x18
        push(cpu.registers.pc);
        cpu.registers.pc = 0x0018;
        cycles = 16;
        break;
    case 0xE0: // LD (0xFF00 + n), A
        mmu_write(IO + operand, cpu.registers.a);
        cycles = 12;
        break;
    case 0xE1: // POP HL
        cpu.registers.hl = pop();
        cycles = 12;
        break;
    case 0xE2: // LD (0xFF00 + C),A
        mmu_write(IO + cpu.registers.c, cpu.registers.a);
        cycles = 8;
        break;
    case 0xE5: // PUSH HL
        push(cpu.registers.hl);
        cycles = 16;
        break;
    case 0xE6: // AND n
        and(operand);
        cycles = 8;
        break;
    case 0xE7: // RST 0x20
        push(cpu.registers.pc);
        cpu.registers.pc = 0x0020;
        cycles = 16;
        break;
    case 0xE8:; // ADD SP, n
        result = cpu.registers.sp + (s_byte_t) operand;
        (((cpu.registers.sp & 0xFF) + ((s_byte_t) operand & 0xFF)) & 0x100) == 0x100 ? SET_FLAG(FLAG_C) : RESET_FLAG(FLAG_C);
        (((cpu.registers.sp & 0x0F) + ((s_byte_t) operand & 0x0F)) & 0x10) == 0x10 ? SET_FLAG(FLAG_H) : RESET_FLAG(FLAG_H);
        cpu.registers.sp = result & 0xFFFF;
        RESET_FLAG(FLAG_N | FLAG_Z);
        cycles = 16;
        break;
    case 0xE9: // JP HL
        cpu.registers.pc = cpu.registers.hl;
        cycles = 4;
        break;
    case 0xEA: // LD (nn), A
        mmu_write(operand, cpu.registers.a);
        cycles = 16;
        break;
    case 0xEE: // XOR n
        xor(operand);
        cycles = 8;
        break;
    case 0xEF: // RST 0x28
        push(cpu.registers.pc);
        cpu.registers.pc = 0x0028;
        cycles = 16;
        break;
    case 0xF0: // LD A, (0xFF00 + n)
        cpu.registers.a = mmu_read(IO + operand);
        cycles = 12;
        break;
    case 0xF1: // POP AF
        cpu.registers.af = pop();
        // clear lower nibble of cpu.registers.f because it can only retreive its flags (most significant nibble)
        cpu.registers.f &= 0xF0;
        cycles = 12;
        break;
    case 0xF2: // LD A,(0xFF00 + C)
        cpu.registers.a = mmu_read(IO + cpu.registers.c);
        cycles = 8;
        break;
    case 0xF3: // DI
        cpu.ime = 0;
        cycles = 4;
        break;
    case 0xF5: // PUSH AF
        push(cpu.registers.af);
        cycles = 16;
        break;
    case 0xF6: // OR n
        or(operand);
        cycles = 8;
        break;
    case 0xF7: // RST 0x30
        push(cpu.registers.pc);
        cpu.registers.pc = 0x0030;
        cycles = 16;
        break;
    case 0xF8:; // LD HL, SP+n
        result = cpu.registers.sp + (s_byte_t) operand;
        (((cpu.registers.sp & 0xFF) + ((s_byte_t) operand & 0xFF)) & 0x100) == 0x100 ? SET_FLAG(FLAG_C) : RESET_FLAG(FLAG_C);
        (((cpu.registers.sp & 0x0F) + ((s_byte_t) operand & 0x0F)) & 0x10) == 0x10 ? SET_FLAG(FLAG_H) : RESET_FLAG(FLAG_H);
        cpu.registers.hl = result & 0xFFFF;
        RESET_FLAG(FLAG_N | FLAG_Z);
        cycles = 12;
        break;
    case 0xF9: // LD SP, HL
        cpu.registers.sp = cpu.registers.hl;
        cycles = 8;
        break;
    case 0xFA: // LD A, (nn)
        cpu.registers.a = mmu_read(operand);
        cycles = 16;
        break;
    case 0xFB: // EI
        cpu.ime = 1;
        cycles = 4;
        break;
    case 0xFE: // CP n
        cp(operand);
        cycles = 8;
        break;
    case 0xFF: // RST 0x38
        push(cpu.registers.pc);
        cpu.registers.pc = 0x0038;
        cycles = 16;
        break;
    default:;
        char buf[32];
        snprintf(buf, sizeof(buf), instructions[opcode].name, operand);
        printf("ERROR: opcode %02X: %s\n", opcode, buf);
        exit(EXIT_FAILURE);
        break;
    }

    return cycles;
}

#ifdef DEBUG
static void print_trace(void) {
    byte_t opcode = mmu_read(cpu.registers.pc);
    byte_t operand_size = instructions[mmu_read(cpu.registers.pc)].operand_size;
    if (operand_size == 0) {
        printf("A:%02x F:%c%c%c%c BC:%04x DE:%04x HL:%04x SP:%04x PC:%04x | %02x        %s\n", cpu.registers.a, CHECK_FLAG(FLAG_Z) ? 'Z' : '-', CHECK_FLAG(FLAG_N) ? 'N' : '-', CHECK_FLAG(FLAG_H) ? 'H' : '-', CHECK_FLAG(FLAG_C) ? 'C' : '-', cpu.registers.bc, cpu.registers.de, cpu.registers.hl, cpu.registers.sp, cpu.registers.pc, mem[cpu.registers.pc], instructions[opcode].name);
    } else if (operand_size == 1) {
        printf("A:%02x F:%c%c%c%c BC:%04x DE:%04x HL:%04x SP:%04x PC:%04x | %02x %02x     %s\n", cpu.registers.a, CHECK_FLAG(FLAG_Z) ? 'Z' : '-', CHECK_FLAG(FLAG_N) ? 'N' : '-', CHECK_FLAG(FLAG_H) ? 'H' : '-', CHECK_FLAG(FLAG_C) ? 'C' : '-', cpu.registers.bc, cpu.registers.de, cpu.registers.hl, cpu.registers.sp, cpu.registers.pc, mem[cpu.registers.pc], mem[cpu.registers.pc + 1], instructions[opcode].name);
    } else {
        printf("A:%02x F:%c%c%c%c BC:%04x DE:%04x HL:%04x SP:%04x PC:%04x | %02x %02x %02x  %s\n", cpu.registers.a, CHECK_FLAG(FLAG_Z) ? 'Z' : '-', CHECK_FLAG(FLAG_N) ? 'N' : '-', CHECK_FLAG(FLAG_H) ? 'H' : '-', CHECK_FLAG(FLAG_C) ? 'C' : '-', cpu.registers.bc, cpu.registers.de, cpu.registers.hl, cpu.registers.sp, cpu.registers.pc, mem[cpu.registers.pc], mem[cpu.registers.pc + 1], mem[cpu.registers.pc + 2], instructions[opcode].name);
    }
}
#endif

static int cpu_handle_interrupts(void) {
    // wake cpu if there is one (or more) interrupt
    if (cpu.halt && (mmu.mem[IE] & mmu.mem[IF]))
        cpu.halt = 0;

    // cpu_ime is truly enabled when its value is 2 to emulate the EI delay by one instruction
    if (cpu.ime == 1) {
        cpu.ime++;
    } else if (cpu.ime == 2 && (mmu.mem[IE] & mmu.mem[IF])) { // if cpu_ime is enabled and at least 1 interrupt is enabled and fired
        cpu.ime = 0;
        push(cpu.registers.pc);

        if (CHECK_BIT(mmu.mem[IF], IRQ_VBLANK)) {
            RESET_BIT(mmu.mem[IF], IRQ_VBLANK);
            cpu.registers.pc = 0x0040;
        } else if (CHECK_BIT(mmu.mem[IF], IRQ_STAT)) {
            RESET_BIT(mmu.mem[IF], IRQ_STAT);
            cpu.registers.pc = 0x0048;
        } else if (CHECK_BIT(mmu.mem[IF], IRQ_TIMER)) {
            RESET_BIT(mmu.mem[IF], IRQ_TIMER);
            cpu.registers.pc = 0x0050;
        } else if (CHECK_BIT(mmu.mem[IF], IRQ_SERIAL)) {
            RESET_BIT(mmu.mem[IF], IRQ_SERIAL);
            cpu.registers.pc = 0x0058;
        } else if (CHECK_BIT(mmu.mem[IF], IRQ_JOYPAD)) {
            RESET_BIT(mmu.mem[IF], IRQ_JOYPAD);
            cpu.registers.pc = 0x0060;
        }

        return 20;
    }

    return 0;
}

void cpu_request_interrupt(int irq) {
    SET_BIT(mmu.mem[IF], irq);
}

int cpu_step(void) {
    int interrupts_cycles = cpu_handle_interrupts();

    if (cpu.halt) return 4 + interrupts_cycles;

    #ifdef DEBUG
    print_trace();
    #endif

    byte_t opcode = mmu_read(cpu.registers.pc);
    cpu.registers.pc++;
    word_t operand = 0; // initialize to 0 to shut gcc warnings
    switch (instructions[opcode].operand_size) {
    case 1:
        operand = mmu_read(cpu.registers.pc);
        break;
    case 2:
        operand = mmu_read(cpu.registers.pc) | mmu_read(cpu.registers.pc + 1) << 8;
        break;
    }
    cpu.registers.pc += instructions[opcode].operand_size;

    return exec_opcode(opcode, operand) + interrupts_cycles;
}

void cpu_init(void) {
    cpu = (cpu_t) { 0 };
}
