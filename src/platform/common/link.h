#pragma once

#include "../../core/core.h"

void link_cancel(void);

int link_start_server(const char *port);

int link_connect_to_server(const char *address, const char *port);

bool link_init_transfer(int sfd, gbmulator_t *emu, gbmulator_t **linked_emu);

/**
 * @return 0 if connection is lost, else 1
 */
bool link_exchange_joypad(int sfd, gbmulator_t *emu, gbmulator_t *linked_emu);
