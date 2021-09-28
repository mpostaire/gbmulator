#include "timer.h"
#include "mem.h"
#include "cpu.h"

int div_counter = 0;
int tima_counter = 0;

void timer_step(int cycles) {
    // DIV register incremented at 16384 Hz (every 256 cycles)
    div_counter += cycles;
    if (div_counter >= 256) {
        div_counter = 0;
        mem[DIV]++; // force increment (mem_write would set this address to 0)
    }

    // if timer is enabled
    if (CHECK_BIT(mem[TAC], 2)) {
        // get frequency of TIMA increment
        int max_cycles = mem[TAC] & 0x03; // get bits 1-0
        if (max_cycles == 0) {
            max_cycles = 1024;
        } else if (max_cycles == 1) {
            max_cycles = 16;
        } else if (max_cycles == 2) {
            max_cycles = 64;
        } else if (max_cycles == 3) {
            max_cycles = 256;
        }

        tima_counter += cycles;
        if (tima_counter >= max_cycles) {
            if (mem[TIMA] == 0xFF) { // about to overflow
                // TODO If a TMA write is executed on the same cycle as the content of TMA is transferred to TIMA due to a timer overflow,
                //      the old value is transferred to TIMA.
                mem[TIMA] = mem[TMA];
                cpu_request_interrupt(IRQ_TIMER);
            } else {
                mem[TIMA]++;
            }
        }
    }
}
