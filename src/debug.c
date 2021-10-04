#include <stdio.h>

#include "debug.h"
#include "types.h"
#include "cpu.h"
#include "mem.h"

void print_trace() {
    byte_t opcode = mem_read(registers.pc);
    byte_t operand_size = instructions[mem_read(registers.pc)].operand_size;
    // char buf[32];
    // snprintf(buf, sizeof(buf), instructions[opcode].name, mem[registers.pc + 1]);
    if (operand_size == 0) {
        printf("A:%02x F:%c%c%c%c BC:%04x DE:%04x HL:%04x SP:%04x PC:%04x (cy: %ld) ppu:+0 |[00]0x0100 %02x        %s\n", registers.a, CHECK_FLAG(FLAG_Z) ? 'Z' : '-', CHECK_FLAG(FLAG_N) ? 'N' : '-', CHECK_FLAG(FLAG_H) ? 'H' : '-', CHECK_FLAG(FLAG_C) ? 'C' : '-', registers.bc, registers.de, registers.hl, registers.sp, registers.pc, total_cycles, mem[registers.pc], instructions[opcode].name);
    } else if (operand_size == 1) {
        printf("A:%02x F:%c%c%c%c BC:%04x DE:%04x HL:%04x SP:%04x PC:%04x (cy: %ld) ppu:+0 |[00]0x0100 %02x %02x     %s\n", registers.a, CHECK_FLAG(FLAG_Z) ? 'Z' : '-', CHECK_FLAG(FLAG_N) ? 'N' : '-', CHECK_FLAG(FLAG_H) ? 'H' : '-', CHECK_FLAG(FLAG_C) ? 'C' : '-', registers.bc, registers.de, registers.hl, registers.sp, registers.pc, total_cycles, mem[registers.pc], mem[registers.pc + 1], instructions[opcode].name);
    } else {
        printf("A:%02x F:%c%c%c%c BC:%04x DE:%04x HL:%04x SP:%04x PC:%04x (cy: %ld) ppu:+0 |[00]0x0100 %02x %02x %02x  %s\n", registers.a, CHECK_FLAG(FLAG_Z) ? 'Z' : '-', CHECK_FLAG(FLAG_N) ? 'N' : '-', CHECK_FLAG(FLAG_H) ? 'H' : '-', CHECK_FLAG(FLAG_C) ? 'C' : '-', registers.bc, registers.de, registers.hl, registers.sp, registers.pc, total_cycles, mem[registers.pc], mem[registers.pc + 1], mem[registers.pc + 2], instructions[opcode].name);
    }
}
