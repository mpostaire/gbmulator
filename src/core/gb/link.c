#include <stdlib.h>

#include "gb_priv.h"

void link_set_clock(gb_t *gb) {
    // double speed is handled by the gb_step() function
    if (gb->mode == GB_MODE_CGB && CHECK_BIT(gb->mmu->io_registers[IO_SC], 1))
        gb->link->max_clock_cycles = GB_CPU_FREQ / 262144;
    else
        gb->link->max_clock_cycles = GB_CPU_FREQ / 8192;
}

void link_init(gb_t *gb) {
    gb->link = xcalloc(1, sizeof(*gb->link));
    link_set_clock(gb);
}

void link_quit(gb_t *gb) {
    if (gb->link->linked_device.type != LINK_TYPE_NONE)
        gb_link_disconnect(gb);
    free(gb->link);
}

uint8_t gb_linked_shift_bit(void *device, uint8_t in_bit) {
    gb_t *gb = device;
    uint8_t out_bit = GET_BIT(gb->mmu->io_registers[IO_SB], 7);
    gb->mmu->io_registers[IO_SB] <<= 1;
    CHANGE_BIT(gb->mmu->io_registers[IO_SB], 0, in_bit);
    return out_bit;
}

void gb_linked_data_received(void *device) {
    gb_t *gb = device;
    RESET_BIT(gb->mmu->io_registers[IO_SC], 7);
    CPU_REQUEST_INTERRUPT(gb, IRQ_SERIAL);
}

void link_step(gb_t *gb) {
    gb_link_t *link = gb->link;
    gb_mmu_t *mmu = gb->mmu;

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
        if (link->linked_device.type != LINK_TYPE_NONE) {
            uint8_t this_bit = GET_BIT(mmu->io_registers[IO_SB], 7);
            // transfer this gb bit to linked device
            other_bit = link->linked_device.shift_bit(link->linked_device.device, this_bit);
        }

        // transfer linked_device bit (other bit) to this gb
        gb_linked_shift_bit(gb, other_bit);
    } else { // transfer is done (all bits were shifted)
        link->bit_shift_counter = 0;

        if (link->linked_device.type != LINK_TYPE_NONE)
            link->linked_device.data_received(link->linked_device.device);

        gb_linked_data_received(gb);
    }
}
