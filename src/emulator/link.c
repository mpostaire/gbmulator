#include <stdlib.h>

#include "emulator.h"
#include "cpu.h"
#include "mmu.h"
#include "serialize.h"

void link_set_clock(emulator_t *emu) {
    // double speed is handled by the emulator_step() function
    if (emu->mode == CGB && CHECK_BIT(emu->mmu->mem[SC], 1))
        emu->link->max_clock_cycles = GB_CPU_FREQ / 262144;
    else
        emu->link->max_clock_cycles = GB_CPU_FREQ / 8192;
}

void link_init(emulator_t *emu) {
    emu->link = xcalloc(1, sizeof(link_t));
    link_set_clock(emu);
}

void link_quit(emulator_t *emu) {
    if (emu->link->other_emu)
        emulator_link_disconnect(emu);
    free(emu->link);
}

void link_step(emulator_t *emu) {
    link_t *link = emu->link;
    mmu_t *mmu = emu->mmu;

    link->cycles += 4; // 4 cycles per step

    if (link->cycles >= link->max_clock_cycles) {
        link->cycles -= link->max_clock_cycles; // keep leftover cycles (if any)

        // transfer requested / in progress with internal clock (this emu is the master of the connection)
        // --> the master emulator also does the work for the slave so we don't have to handle the case
        //     where this emu is the slave
        if (CHECK_BIT(mmu->mem[SC], 7) && CHECK_BIT(mmu->mem[SC], 0)) {
            if (link->bit_counter < 8) { // emulate 8 bit shifting
                link->bit_counter++;

                byte_t other_bit = 0xFF; // this is 0xFF if no other_emu connected

                if (link->other_emu) {
                    other_bit = GET_BIT(link->other_emu->mmu->mem[SB], 7);
                    byte_t this_bit = GET_BIT(mmu->mem[SB], 7);

                    // transfer this emu bit to other_emu
                    link->other_emu->mmu->mem[SB] <<= 1;
                    CHANGE_BIT(link->other_emu->mmu->mem[SB], 0, this_bit);
                }

                // transfer other_emu bit to this emu
                mmu->mem[SB] <<= 1;
                CHANGE_BIT(mmu->mem[SB], 0, other_bit);
            } else { // transfer is done (all bits were shifted)
                link->bit_counter = 0;

                RESET_BIT(mmu->mem[SC], 7);
                CPU_REQUEST_INTERRUPT(emu, IRQ_SERIAL);

                if (link->other_emu) {
                    RESET_BIT(link->other_emu->mmu->mem[SC], 7);
                    CPU_REQUEST_INTERRUPT(link->other_emu, IRQ_SERIAL);
                }
            }
        }
    }
}

#define SERIALIZED_MEMBERS \
    X(cycles)              \
    X(max_clock_cycles)    \
    X(bit_counter)

#define X(value) SERIALIZED_LENGTH(value);
SERIALIZED_SIZE_FUNCTION(link_t, link,
    SERIALIZED_MEMBERS
)
#undef X

#define X(value) SERIALIZE(value);
SERIALIZER_FUNCTION(link_t, link,
    SERIALIZED_MEMBERS
)
#undef X

#define X(value) UNSERIALIZE(value);
UNSERIALIZER_FUNCTION(link_t, link,
    SERIALIZED_MEMBERS
)
#undef X
