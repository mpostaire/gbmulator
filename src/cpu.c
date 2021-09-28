#include <stdio.h>
#include <stdlib.h>

#include "cpu.h"
#include "mem.h"

struct registers registers;
// interrupt master enable
int ime = 0;

const instruction_t instructions[256] = {
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
    {"UNKNOWN", 0},               // 0xD3
    {"CALL NC, %04X", 2},         // 0xD4
    {"PUSH DE", 0},               // 0xD5
    {"SUB A, %02X", 1},           // 0xD6
    {"RST 0x10", 0},              // 0xD7
    {"RET C", 0},                 // 0xD8
    {"RETI", 0},                  // 0xD9
    {"JP C, %04X", 2},            // 0xDA
    {"UNKNOWN", 0},               // 0xDB
    {"CALL C, %04X", 2},          // 0xDC
    {"UNKNOWN", 0},               // 0xDD
    {"SBC %02X", 1},              // 0xDE
    {"RST 0x18", 0},              // 0xDF
    {"LD (0xFF00 + %02X), A", 1}, // 0xE0
    {"POP HL", 0},                // 0xE1
    {"LD (0xFF00 + C), A", 0},    // 0xE2
    {"UNKNOWN", 0},               // 0xE3
    {"UNKNOWN", 0},               // 0xE4
    {"PUSH HL", 0},               // 0xE5
    {"AND %02X", 1},              // 0xE6
    {"RST 0x20", 0},              // 0xE7
    {"ADD SP,%02X", 1},           // 0xE8
    {"JP (HL)", 0},               // 0xE9
    {"LD (%04X), A", 2},          // 0xEA
    {"UNKNOWN", 0},               // 0xEB
    {"UNKNOWN", 0},               // 0xEC
    {"UNKNOWN", 0},               // 0xED
    {"XOR %02X", 1},              // 0xEE
    {"RST 0x28", 0},              // 0xEF
    {"LD A, (0xFF00 + %02X)", 1}, // 0xF0
    {"POP AF", 0},                // 0xF1
    {"LD A, (0xFF00 + C)", 0},    // 0xF2
    {"DI", 0},                    // 0xF3
    {"UNKNOWN", 0},               // 0xF4
    {"PUSH AF", 0},               // 0xF5
    {"OR %02X", 1},               // 0xF6
    {"RST 0x30", 0},              // 0xF7
    {"LD HL, SP+%02X", 1},        // 0xF8
    {"LD SP, HL", 0},             // 0xF9
    {"LD A, (%04X)", 2},          // 0xFA
    {"EI", 0},                    // 0xFB
    {"UNKNOWN", 0},               // 0xFC
    {"UNKNOWN", 0},               // 0xFD
    {"CP %02X", 1},               // 0xFE
    {"RST 0x38", 0},              // 0xFF
};

const instruction_t extended_instructions[256] = {
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
    registers.sp--;
    mem_write(registers.sp, hi);
    registers.sp--;
    mem_write(registers.sp, lo);
}

static word_t pop() {
    word_t data = mem_read(registers.sp) | mem_read(registers.sp + 1) << 8;
    registers.sp += 2;
    return data;
}

static void xor(byte_t reg) {
    registers.a ^= reg;
    RESET_FLAG(FLAG_C | FLAG_H | FLAG_N);
    registers.a ? RESET_FLAG(FLAG_Z) : SET_FLAG(FLAG_Z);
}

static void bit(byte_t reg, byte_t pos) {
	CHECK_BIT(reg, pos) ? RESET_FLAG(FLAG_Z) : SET_FLAG(FLAG_Z);
	RESET_FLAG(FLAG_N);
	SET_FLAG(FLAG_H);
}

// TODO half carry (FLAG_H) may be implemented wrong
static byte_t inc(byte_t reg) {
    // if lower nibble of reg is at max value, there will be a half carry after we increment.
    (reg & 0x0F) == 0x0F ? SET_FLAG(FLAG_H) : RESET_FLAG(FLAG_H);
    reg++;
    reg ? RESET_FLAG(FLAG_Z) : SET_FLAG(FLAG_Z);
    RESET_FLAG(FLAG_N);
    return reg;
}

// TODO half carry (FLAG_H) may be implemented wrong
static byte_t dec(byte_t reg) {
    // if lower nibble of reg is 0, there will be a half carry after we decrement (not sure if this is a sufficient condition)
    (reg & 0x0F) == 0x00 ? SET_FLAG(FLAG_H) : RESET_FLAG(FLAG_H);
    reg--;
    reg ? RESET_FLAG(FLAG_Z) : SET_FLAG(FLAG_Z);
    SET_FLAG(FLAG_N);
    return reg;
}

static word_t rl(word_t reg) {
	int carry = CHECK_FLAG(FLAG_C) ? 1 : 0;
    // set new carry to the value of leftmost bit
	reg & 0x80 ? SET_FLAG(FLAG_C) : RESET_FLAG(FLAG_C);
    // rotate left
	reg <<= 1;
    // set leftmost bit to the value of the old carry
	reg += carry;

	reg ? RESET_FLAG(FLAG_Z) : SET_FLAG(FLAG_Z);
	RESET_FLAG(FLAG_N | FLAG_H);
	return reg;
}

// TODO half carry (FLAG_H) may be implemented wrong (maybe all flags here are wrong)
void cp(byte_t reg) {
    registers.a == reg ? SET_FLAG(FLAG_N) : RESET_FLAG(FLAG_Z);
    reg > registers.a ? SET_FLAG(FLAG_C) : RESET_FLAG(FLAG_C);
    (reg & 0x0f) > (registers.a & 0x0f) ? SET_FLAG(FLAG_H) : RESET_FLAG(FLAG_H);
    SET_FLAG(FLAG_N);
}

static int cpu_exec_extended_opcode(byte_t opcode) {
    int cycles;

    switch (opcode) {
    case 0x10: // RL B
        registers.b = rl(registers.b);
        cycles = 8;
        break;
    case 0x11: // RL C
        registers.c = rl(registers.c);
        cycles = 8;
        break;
    case 0x12: // RL D
        registers.d = rl(registers.d);
        cycles = 8;
        break;
    case 0x13: // RL E
        registers.e = rl(registers.e);
        cycles = 8;
        break;
    case 0x14: // RL H
        registers.h = rl(registers.h);
        cycles = 8;
        break;
    case 0x15: // RL L
        registers.l = rl(registers.l);
        cycles = 8;
        break;
    case 0x16: // RL (HL)
        mem_write(registers.hl, rl(mem_read(registers.hl)));
        cycles = 16;
        break;
    case 0x17: // RL A
        registers.a = rl(registers.a);
        cycles = 8;
        break;
    case 0x7C: // BIT 7, H
        bit(registers.h, 7);
        cycles = 8;
        break;
    default:
        exit(EXIT_FAILURE);
        break;
    }

    return cycles;
}

static int cpu_exec_opcode(byte_t opcode, word_t operand) {
    int cycles;

    switch (opcode) {
    case 0x00: // NOP
        cycles = 4;
        break;
    case 0x01: // LD BC, nn
        registers.bc = operand;
        cycles = 12;
        break;
    case 0x02: // LD (BC),A
        mem_write(registers.bc, registers.a);
        cycles = 8;
        break;
    case 0x03: // INC BC
        registers.bc++;
        cycles = 8;
        break;
    case 0x04: // INC B
        registers.b = inc(registers.b);
        cycles = 4;
        break;
    case 0x05: // DEC B
        registers.b = dec(registers.b);
        cycles = 4;
        break;
    case 0x06: // LD B, n
        registers.b = operand;
        cycles = 8;
        break;
    // case 0x08: // LD (nn), SP TODO this may be wrong
    //     registers.sp = operand;
    //     cycles = 20;
    //     break;
    case 0x0A: // LD A,(BC)
        registers.a = mem_read(registers.bc);
        cycles = 8;
        break;
    case 0x0C: // INC C
        registers.c = inc(registers.c);
        cycles = 4;
        break;
    case 0x0D: // DEC C
        registers.c = dec(registers.c);
        cycles = 4;
        break;
    case 0x0E: // LD C, n
        registers.c = operand;
        cycles = 8;
        break;
    case 0x11: // LD DE, nn
        registers.de = operand;
        cycles = 12;
        break;
    case 0x12: // LD (DE),A
        mem_write(registers.de, registers.a);
        cycles = 8;
        break;
    case 0x13: // INC DE
        registers.de++;
        cycles = 8;
        break;
    case 0x14: // INC D
        registers.d = inc(registers.d);
        cycles = 4;
        break;
    case 0x15: // DEC D
        registers.d = dec(registers.d);
        cycles = 4;
        break;
    case 0x16: // LD D, n
        registers.d = operand;
        cycles = 8;
        break;
    case 0x17: // RLA
        registers.a = rl(registers.a);
        RESET_FLAG(FLAG_Z);
        cycles = 4;
        break;
    case 0x1A: // LD A,(DE)
        registers.a = mem_read(registers.de);
        cycles = 8;
        break;
    case 0x1C: // INC E
        registers.e = inc(registers.e);
        cycles = 4;
        break;
    case 0x1D: // DEC E
        registers.e = dec(registers.e);
        cycles = 4;
        break;
    case 0x1E: // LD E, n
        registers.d = operand;
        cycles = 8;
        break;
    case 0x20: // JR NZ, n
        if (!CHECK_FLAG(FLAG_Z)) {
            registers.pc += (s_byte_t) operand;
            cycles = 12;
        } else {
            cycles = 8;
        }
        break;
    case 0x21: // LD HL, nn
        registers.hl = operand;
        cycles = 12;
        break;
    case 0x22: // LDI (HL), A
        mem_write(registers.hl, registers.a);
        registers.hl++;
        cycles = 8;
        break;
    case 0x23: // INC HL
        registers.hl++;
        cycles = 8;
        break;
    case 0x24: // INC H
        registers.h = inc(registers.h);
        cycles = 4;
        break;
    case 0x25: // DEC H
        registers.h = dec(registers.h);
        cycles = 4;
        break;
    case 0x26: // LD H, n
        registers.h = operand;
        cycles = 8;
        break;
    case 0x28: // JR Z, n
        if (CHECK_FLAG(FLAG_Z)) {
            registers.pc += (s_byte_t) operand;
            cycles = 12;
        } else {
            cycles = 8;
        }
        break;
    case 0x2A: // LDI A, (HL)
        registers.a = mem_read(registers.hl);
        registers.hl++;
        cycles = 8;
        break;
    case 0x2C: // INC L
        registers.l = inc(registers.l);
        cycles = 4;
        break;
    case 0x2D: // DEC L
        registers.l = dec(registers.l);
        cycles = 4;
        break;
    case 0x2E: // LD L, n
        registers.l = operand;
        cycles = 8;
        break;
    case 0x30: // JR NC, n
        if (!CHECK_FLAG(FLAG_C)) {
            registers.pc += (s_byte_t) operand;
            cycles = 12;
        } else {
            cycles = 8;
        }
        break;
    case 0x31: // LD SP,nn
        registers.sp = operand;
        cycles = 12;
        break;
    case 0x32: // LDD (HL),A
        mem_write(registers.hl, registers.a);
        registers.hl--;
        cycles = 8;
        break;
    case 0x33: // INC SP
        registers.sp++;
        cycles = 8;
        break;
    case 0x34: // INC (HL)
        mem_write(registers.hl, inc(mem_read(registers.hl)));
        cycles = 12;
        break;
    case 0x35: // DEC (HL)
        mem_write(registers.hl, dec(mem_read(registers.hl)));
        cycles = 12;
        break;
    case 0x36: // LD (HL),n
        mem_write(registers.hl, operand);
        cycles = 12;
        break;
    case 0x38: // JR C, n
        if (CHECK_FLAG(FLAG_C)) {
            registers.pc += (s_byte_t) operand;
            cycles = 12;
        } else {
            cycles = 8;
        }
        break;
    case 0x3C: // INC A
        registers.a = inc(registers.a);
        cycles = 4;
        break;
    case 0x3D: // DEC A
        registers.a = dec(registers.a);
        cycles = 4;
        break;
    case 0x3E: // LD A,n
        registers.a = operand;
        cycles = 8;
        break;
    case 0x40: // LD B,B
        // registers.b = registers.b;
        cycles = 4;
        break;
    case 0x41: // LD B,C
        registers.b = registers.c;
        cycles = 4;
        break;
    case 0x42: // LD B,D
        registers.b = registers.b;
        cycles = 4;
        break;
    case 0x43: // LD B,E
        registers.b = registers.e;
        cycles = 4;
        break;
    case 0x44: // LD B,H
        registers.b = registers.h;
        cycles = 4;
        break;
    case 0x45: // LD B,L
        registers.b = registers.l;
        cycles = 4;
        break;
    case 0x46: // LD B,(HL)
        registers.b = mem_read(registers.hl);
        cycles = 8;
        break;
    case 0x47: // LD B,A
        registers.b = registers.a;
        cycles = 4;
        break;
    case 0x48: // LD C,B
        registers.c = registers.b;
        cycles = 4;
        break;
    case 0x49: // LD C,C
        // registers.c = registers.c;
        cycles = 4;
        break;
    case 0x4A: // LD C,D
        registers.c = registers.d;
        cycles = 4;
        break;
    case 0x4B: // LD C,E
        registers.c = registers.e;
        cycles = 4;
        break;
    case 0x4C: // LD C,H
        registers.c = registers.h;
        cycles = 4;
        break;
    case 0x4D: // LD C,L
        registers.c = registers.l;
        cycles = 4;
        break;
    case 0x4E: // LD C,(HL)
        registers.c = mem_read(registers.hl);
        cycles = 8;
        break;
    case 0x4F: // LD C,A
        registers.c = registers.a;
        cycles = 4;
        break;
    case 0x50: // LD D,B
        registers.d = registers.b;
        cycles = 4;
        break;
    case 0x51: // LD D,C
        registers.d = registers.c;
        cycles = 4;
        break;
    case 0x52: // LD D,D
        // registers.d = registers.d;
        cycles = 4;
        break;
    case 0x53: // LD D,E
        registers.d = registers.e;
        cycles = 4;
        break;
    case 0x54: // LD D,H
        registers.d = registers.h;
        cycles = 4;
        break;
    case 0x55: // LD D,L
        registers.d = registers.l;
        cycles = 4;
        break;
    case 0x56: // LD D,(HL)
        registers.d = mem_read(registers.hl);
        cycles = 8;
        break;
    case 0x57: // LD D,A
        registers.d = registers.a;
        cycles = 4;
        break;
    case 0x58: // LD E,B
        registers.e = registers.b;
        cycles = 4;
        break;
    case 0x59: // LD E,C
        registers.e = registers.c;
        cycles = 4;
        break;
    case 0x5A: // LD E,D
        registers.e = registers.d;
        cycles = 4;
        break;
    case 0x5B: // LD E,E
        // registers.e = registers.e;
        cycles = 4;
        break;
    case 0x5C: // LD E,H
        registers.e = registers.h;
        cycles = 4;
        break;
    case 0x5D: // LD E,L
        registers.e = registers.l;
        cycles = 4;
        break;
    case 0x5E: // LD E,(HL)
        registers.e = mem_read(registers.hl);
        cycles = 8;
        break;
    case 0x5F: // LD E,A
        registers.e = registers.a;
        cycles = 4;
        break;
    case 0x60: // LD H,B
        registers.h = registers.b;
        cycles = 4;
        break;
    case 0x61: // LD H,C
        registers.h = registers.c;
        cycles = 4;
        break;
    case 0x62: // LD H,D
        registers.h = registers.d;
        cycles = 4;
        break;
    case 0x63: // LD H,E
        registers.h = registers.e;
        cycles = 4;
        break;
    case 0x64: // LD H,H
        // registers.h = registers.h;
        cycles = 4;
        break;
    case 0x65: // LD H,L
        registers.h = registers.l;
        cycles = 4;
        break;
    case 0x66: // LD H,(HL)
        registers.h = mem_read(registers.hl);
        cycles = 8;
        break;
    case 0x67: // LD H,A
        registers.h = registers.a;
        cycles = 4;
        break;
    case 0x68: // LD L,B
        registers.l = registers.b;
        cycles = 4;
        break;
    case 0x69: // LD L,C
        registers.l = registers.c;
        cycles = 4;
        break;
    case 0x6A: // LD L,D
        registers.l = registers.d;
        cycles = 4;
        break;
    case 0x6B: // LD L,E
        registers.l = registers.e;
        cycles = 4;
        break;
    case 0x6C: // LD L,H
        registers.l = registers.h;
        cycles = 4;
        break;
    case 0x6D: // LD L,L
        // registers.l = registers.l;
        cycles = 4;
        break;
    case 0x6E: // LD L,(HL)
        registers.l = mem_read(registers.hl);
        cycles = 8;
        break;
    case 0x6F: // LD L,A
        registers.l = registers.a;
        cycles = 4;
        break;
    case 0x70: // LD (HL),B
        mem_write(registers.hl, registers.b);
        cycles = 8;
        break;
    case 0x71: // LD (HL),C
        mem_write(registers.hl, registers.c);
        cycles = 8;
        break;
    case 0x72: // LD (HL),D
        mem_write(registers.hl, registers.d);
        cycles = 8;
        break;
    case 0x73: // LD (HL),E
        mem_write(registers.hl, registers.e);
        cycles = 8;
        break;
    case 0x74: // LD (HL),H
        mem_write(registers.hl, registers.h);
        cycles = 8;
        break;
    case 0x75: // LD (HL),L
        mem_write(registers.hl, registers.l);
        cycles = 8;
        break;
    case 0x77: // LD (HL),A
        mem_write(registers.hl, registers.a);
        cycles = 8;
        break;
    case 0x78: // LD A,B
        registers.a = registers.b;
        cycles = 4;
        break;
    case 0x79: // LD A,C
        registers.a = registers.c;
        cycles = 4;
        break;
    case 0x7A: // LD A,D
        registers.a = registers.d;
        cycles = 4;
        break;
    case 0x7B: // LD A,E
        registers.a = registers.e;
        cycles = 4;
        break;
    case 0x7C: // LD A,H
        registers.a = registers.h;
        cycles = 4;
        break;
    case 0x7D: // LD A,L
        registers.a = registers.l;
        cycles = 4;
        break;
    case 0x7E: // LD A,(HL)
        registers.a = mem_read(registers.hl);
        cycles = 8;
        break;
    case 0x7F: // LD A,A
        // registers.a = registers.a;
        cycles = 4;
        break;
    case 0xA8: // XOR B
        xor(registers.b);
        cycles = 4;
        break;
    case 0xA9: // XOR C
        xor(registers.c);
        cycles = 4;
        break;
    case 0xAA: // XOR D
        xor(registers.d);
        cycles = 4;
        break;
    case 0xAB: // XOR E
        xor(registers.e);
        cycles = 4;
        break;
    case 0xAC: // XOR H
        xor(registers.h);
        cycles = 4;
        break;
    case 0xAD: // XOR L
        xor(registers.l);
        cycles = 4;
        break;
    case 0xAE: // XOR (HL)
        xor(mem_read(registers.hl));
        cycles = 8;
        break;
    case 0xAF: // XOR A
        xor(registers.a);
        cycles = 4;
        break;
    case 0xB8: // CP B
        cp(registers.b);
        cycles = 4;
        break;
    case 0xB9: // CP C
        cp(registers.c);
        cycles = 4;
        break;
    case 0xBA: // CP D
        cp(registers.d);
        cycles = 4;
        break;
    case 0xBB: // CP E
        cp(registers.e);
        cycles = 4;
        break;
    case 0xBC: // CP H
        cp(registers.h);
        cycles = 4;
        break;
    case 0xBD: // CP L
        cp(registers.l);
        cycles = 4;
        break;
    case 0xBE: // CP (HL)
        cp(mem_read(registers.hl));
        cycles = 8;
        break;
    case 0xBF: // CP A
        cp(registers.a);
        cycles = 4;
        break;
    case 0xC1: // POP BC
        registers.bc = pop();
        cycles = 12;
        break;
    case 0xC3: // JP nn
        registers.pc = operand;
        cycles = 16; // TODO check wich is correct: in GBCPUman.pdf it's only 12 cycles
        break;
    case 0xC5: // PUSH BC
        push(registers.bc);
        cycles = 16;
        break;
    case 0xC9: // RET
        registers.pc = pop();
        cycles = 16; // TODO check wich is correct: in GBCPUman.pdf it's only 8 cycles
        break;
    case 0xCB: // CB nn (prefix instruction)
        return cpu_exec_extended_opcode(operand);
    case 0xCD: // CALL nn
        push(registers.pc);
        registers.pc = operand;
        cycles = 24; // TODO check wich is correct: in GBCPUman.pdf it's only 12 cycles
        break;
    case 0xD1: // POP DE
        registers.de = pop();
        cycles = 12;
        break;
    case 0xD5: // PUSH DE
        push(registers.de);
        cycles = 16;
        break;
    case 0xE0: // LD (0xFF00 + n), A
        mem_write(IO + operand, registers.a);
        cycles = 12;
        break;
    case 0xE1: // POP HL
        registers.hl = pop();
        cycles = 12;
        break;
    case 0xE2: // LD (0xFF00 + C),A
        mem_write(IO + registers.c, registers.a);
        cycles = 8;
        break;
    case 0xE5: // PUSH HL
        push(registers.hl);
        cycles = 16;
        break;
    case 0xEA: // LD (nn),A
        mem_write(operand, registers.a);
        cycles = 16;
        break;
    case 0xEE: // XOR n
        xor(operand);
        cycles = 8;
        break;
    case 0xF0: // LD A, (0xFF00 + n)
        registers.a = mem_read(IO + operand);
        cycles = 12;
        break;
    case 0xF1: // POP AF
        registers.af = pop();
        cycles = 12;
        break;
    case 0xF2: // LD A,(0xFF00 + C)
        registers.a = mem_read(IO + registers.c);
        cycles = 8;
        break;
    case 0xF3: // DI
        ime = 0;
        cycles = 4;
        break;
    case 0xF5: // PUSH AF
        push(registers.af);
        cycles = 16;
        break;
    case 0xFA: // LD A,(nn)
        registers.a = mem_read(operand);
        cycles = 16;
        break;
    case 0xFB: // EI
        ime = 1;
        cycles = 4;
        break;
    case 0xFE: // CP n
        cp(operand);
        cycles = 8;
        break;
    case 0xFF: // RST 0x38
        push(registers.pc);
        registers.pc = 0x0038;
        cycles = 16;
        break;
    default:;
        exit(EXIT_FAILURE);
        break;
    }

    return cycles;
}

void cpu_request_interrupt(int irq) {
    SET_BIT(mem[IF], irq);
}

static int cpu_handle_interrupts() {
    byte_t ienable = mem_read(IE);
    byte_t iflag = mem_read(IF);

    // ime is truly enabled when its value is 2 to emulate the EI delay by one instruction
    if (ime == 1) {
        ime++;
    } else if (ime == 2 && (ienable & iflag)) { // if ime is enabled and at least 1 interrupt is enabled and fired
        ime = 0;
        push(registers.pc);

        if (CHECK_BIT(iflag, IRQ_VBLANK)) {
            RESET_BIT(iflag, IRQ_VBLANK);
            registers.pc = 0x0040;
            puts("\tVBLANK");
        } else if (CHECK_BIT(iflag, IRQ_STAT)) {
            RESET_BIT(iflag, IRQ_STAT);
            registers.pc = 0x0048;
            puts("\tLCD");
        } else if (CHECK_BIT(iflag, IRQ_TIMER)) {
            RESET_BIT(iflag, IRQ_TIMER);
            registers.pc = 0x0050;
            puts("\tTIMER");
        } else if (CHECK_BIT(iflag, IRQ_SERIAL)) {
            RESET_BIT(iflag, IRQ_SERIAL);
            registers.pc = 0x0058;
            puts("\tSERIAL");
        } else if (CHECK_BIT(iflag, IRQ_JOYPAD)) {
            RESET_BIT(iflag, IRQ_JOYPAD);
            registers.pc = 0x0060;
            puts("\tJOYPAD");
        }

        return 20;
    }

    return 0;
}

int cpu_step() {
    int cycles = cpu_handle_interrupts();

    word_t debug_pc = registers.pc;
    word_t debug_sp = registers.sp;
    word_t debug_af = registers.af;
    word_t debug_bc = registers.bc;
    word_t debug_de = registers.de;
    word_t debug_hl = registers.hl;

    byte_t opcode = mem_read(registers.pc);
    registers.pc++;
    word_t operand;
    switch (instructions[opcode].operand_size) {
    case 1:
        operand = mem_read(registers.pc);
        break;
    case 2:
        operand = mem_read(registers.pc) | mem_read(registers.pc + 1) << 8;
        break;
    }
    registers.pc += instructions[opcode].operand_size;
    
    // char buf[32];
    // if (opcode == 0xCB) {
    //     printf("opcode %02X %02X: %s\t\t(pc=%04X, sp=%04X, af=%04X, bc=%04X, de=%04X, hl=%04X) (ime=%d, ie=%02X, if=%02X)\n", opcode, operand, extended_instructions[operand].name, debug_pc, debug_sp, debug_af, debug_bc, debug_de, debug_hl, ime, mem_read(IE), mem_read(IF));
    // } else {
    //     snprintf(buf, sizeof(buf), instructions[opcode].name, operand);
    //     printf("opcode %02X: %s\t\t(pc=%04X, sp=%04X, af=%04X, bc=%04X, de=%04X, hl=%04X) (ime=%d, ie=%02X, if=%02X)\n", opcode, buf, debug_pc, debug_sp, debug_af, debug_bc, debug_de, debug_hl, ime, mem_read(IE), mem_read(IF));
    // }

    return cycles + cpu_exec_opcode(opcode, operand);
}
