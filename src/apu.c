#include "types.h"
#include "mem.h"

const byte_t duty_cycles[4][8] = {
    { 1, 0, 0, 0, 0, 0, 0, 0 },
    { 1, 1, 0, 0, 0, 0, 0, 0 },
    { 1, 1, 1, 1, 0, 0, 0, 0 },
    { 1, 1, 1, 1, 1, 1, 0, 0 }
};

static void channel2_step(int cycles) {
    word_t freq = (mem[NR24] & 0x03) << 8 | mem[NR23];
    freq = 131072 / (2048 - freq);
}

void apu_step(int cycles) {
    channel2_step(cycles);
}
