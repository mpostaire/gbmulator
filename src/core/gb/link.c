#include <stdlib.h>

#include "gb_priv.h"

void link_set_clock(gb_t *gb) {
    // double speed is handled by the gb_step() function
    if (gb->base->opts.mode == GBMULATOR_MODE_GBC && CHECK_BIT(gb->mmu.io_registers[IO_SC], 1))
        gb->link.max_clock_cycles = GB_CPU_FREQ / 262144;
    else
        gb->link.max_clock_cycles = GB_CPU_FREQ / 8192;
}

void link_reset(gb_t *gb) {
    memset(&gb->link, 0, sizeof(gb->link));
    link_set_clock(gb);
}

void link_step(gb_t *gb) {
    gb_link_t *link = &gb->link;
    gb_mmu_t  *mmu  = &gb->mmu;

    link->cycles += 4; // 4 cycles per step
    if (link->cycles < link->max_clock_cycles)
        return;

    // transfer requested / in progress with internal clock (this gb is the master of the connection)
    // --> the master emulator also does the work for the slave so we don't have to handle the case
    //     where this gb is the slave
    uint8_t master_transfer_request = CHECK_BIT(mmu->io_registers[IO_SC], 7) && CHECK_BIT(mmu->io_registers[IO_SC], 0);
    if (!master_transfer_request)
        return;

    if (link->bit_shift_counter < 8) {
        link->bit_shift_counter++;

        uint8_t other_bit = 1; // this is 1 if no device is connected
        if (gb->base->cable.other_device) {
            uint8_t this_bit = GET_BIT(mmu->io_registers[IO_SB], 7);
            // transfer this gb bit to linked device
            other_bit = gb->base->cable.other_device->cable.shift_bit(gb->base->cable.other_device->impl, this_bit);
        }

        // transfer linked_device bit (other bit) to this gb
        gb->base->cable.shift_bit(gb, other_bit);
    } else { // transfer is done (all bits were shifted)
        // TODO maybe both these callbacks are useless --> this condition donest need cb as we can deduce it?
        // TODO important debug link
        link->bit_shift_counter = 0;

        if (gb->base->cable.other_device)
            gb->base->cable.other_device->cable.data_received(gb->base->cable.other_device->impl);

        gb->base->cable.data_received(gb);
    }
}
