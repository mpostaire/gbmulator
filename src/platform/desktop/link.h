#pragma once

#include <gio/gio.h>
#include "../../emulator/emulator.h"

emulator_t *link_communicate(void);

/**
 * This pauses the emulation loop, asynchronoulsy initialize the connection and starts exchanging savestates...
 * Once the connection initialization is done the emulation loop is resumed
 */
void link_setup_connection(GSocketConnection *conn, emulator_t *emu);