#pragma once

#include <gio/gio.h>

/**
 * @return the remotely linked emulator_t pointer if emulation can continue,
 *         NULL if a transfer is in progress and must block emulation.
 */
emulator_t *link_cable(emulator_t *emu, byte_t joypad_state);

void link_setup_connection(GSocketConnection *connection);
