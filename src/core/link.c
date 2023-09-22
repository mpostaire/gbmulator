#include <stdlib.h>

#include "gb_priv.h"
#include "serialize.h"

void link_set_clock(gb_t *gb) {
    // double speed is handled by the gb_step() function
    if (gb->mode == CGB && CHECK_BIT(gb->mmu->io_registers[SC - IO], 1))
        gb->link->max_clock_cycles = GB_CPU_FREQ / 262144;
    else
        gb->link->max_clock_cycles = GB_CPU_FREQ / 8192;
}

void link_init(gb_t *gb) {
    gb->link = xcalloc(1, sizeof(gb_link_t));
    link_set_clock(gb);
}

void link_quit(gb_t *gb) {
    if (gb->link->other_emu)
        gb_link_disconnect(gb);
    free(gb->link);
}

static inline void gb_data_received(gb_t *gb) {
    RESET_BIT(gb->mmu->io_registers[SC - IO], 7);
    CPU_REQUEST_INTERRUPT(gb, IRQ_SERIAL);
}

void link_step(gb_t *gb) {
    gb_link_t *link = gb->link;
    gb_mmu_t *mmu = gb->mmu;

    link->cycles += 4; // 4 cycles per step

    if (link->printer)
        gb_printer_step(link->printer);

    if (link->cycles >= link->max_clock_cycles) {
        link->cycles -= link->max_clock_cycles; // keep leftover cycles (if any)

        // TODO instead of link->other_emu and link->printer pointers, on connection, take a pointer to the link data register of the device
        //      and to tranfer to this pointer dereferenced. Allow callback once transfer of the byte is finished
        //      to request interrupt in slave gameboy or do whatever is needed in slave printer

        // transfer requested / in progress with internal clock (this gb is the master of the connection)
        // --> the master emulator also does the work for the slave so we don't have to handle the case
        //     where this gb is the slave
        if (CHECK_BIT(mmu->io_registers[SC - IO], 7) && CHECK_BIT(mmu->io_registers[SC - IO], 0)) {
            if (link->bit_counter < 8) { // emulate 8 bit shifting
                link->bit_counter++;

                byte_t other_bit = 0xFF; // this is 0xFF if no device is connected

                byte_t *other_data_register = NULL;
                if (link->other_emu)
                    other_data_register = &link->other_emu->mmu->io_registers[SB - IO];
                else if (link->printer)
                    other_data_register = &link->printer->sb;

                if (other_data_register) {
                    other_bit = GET_BIT(*other_data_register, 7);
                    byte_t this_bit = GET_BIT(mmu->io_registers[SB - IO], 7);

                    // transfer this gb bit to linked device
                    *other_data_register <<= 1;
                    CHANGE_BIT(*other_data_register, 0, this_bit);
                }

                // transfer other_emu bit to this gb
                mmu->io_registers[SB - IO] <<= 1;
                CHANGE_BIT(mmu->io_registers[SB - IO], 0, other_bit);
            } else { // transfer is done (all bits were shifted)
                link->bit_counter = 0;

                gb_data_received(gb);

                if (link->other_emu)
                    gb_data_received(link->other_emu);
                else if (link->printer)
                    gb_printer_data_received(link->printer);
            }
        }
    }
}

#define SERIALIZED_MEMBERS \
    X(cycles)              \
    X(max_clock_cycles)    \
    X(bit_counter)

#define X(value) SERIALIZED_LENGTH(value);
SERIALIZED_SIZE_FUNCTION(gb_link_t, link,
    SERIALIZED_MEMBERS
)
#undef X

#define X(value) SERIALIZE(value);
SERIALIZER_FUNCTION(gb_link_t, link,
    SERIALIZED_MEMBERS
)
#undef X

#define X(value) UNSERIALIZE(value);
UNSERIALIZER_FUNCTION(gb_link_t, link,
    SERIALIZED_MEMBERS
)
#undef X
