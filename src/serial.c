#include "cpu.h"
#include "mem.h"

// TODO send udp packets to emulate sending / receiving data

int cycles_counter = 0;
int bit_counter = 0;

void serial_step(int cycles) {
    cycles_counter += cycles;

    if (cycles_counter >= 512) { // 8192 Hz clock
        cycles_counter -= 512; // keep leftover cycles (if any) for next call to serial_step()

        if (CHECK_BIT(mem[SC], 7)) { // transfer requested / in progress
            if (CHECK_BIT(mem[SC], 0)) { // master
                if (bit_counter > 7) {
                    RESET_BIT(mem[SC], 7);
                    cpu_request_interrupt(IRQ_SERIAL);
                    bit_counter = 0;
                } else {
                    // TODO keep shifted bit and send it to an acual emulated gameboy.
                    mem[SB] <<= 1; // shift 1 bit of data to send to the other game boy
                    bit_counter++;
                }
            }
        }
    }
}
