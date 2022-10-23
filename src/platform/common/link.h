#pragma once

#include "emulator/emulator.h"

int link_start_server(const char *port);

int link_connect_to_server(const char *address, const char *port);

int link_init_transfer(int sfd, emulator_t *emu, emulator_t **linked_emu);

/**
 * @return 0 if connection is lost, else 1
 */
int link_exchange_joypad(int sfd, emulator_t *emu, emulator_t *linked_emu);
