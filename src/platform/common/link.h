#pragma once

#include "emulator/emulator.h"

int link_start_server(const char *port, int is_ipv6, int mptcp_enabled);

int link_connect_to_server(const char *address, const char *port, int is_ipv6, int mptcp_enabled);

int link_init_transfer(emulator_t *emu, emulator_t **linked_emu);

/**
 * @return 0 if connection is lost, else 1
 */
int link_exchange_joypad(emulator_t *emu, emulator_t *linked_emu);
