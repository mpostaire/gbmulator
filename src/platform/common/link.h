#pragma once

#include "../../core/core.h"

void link_cancel(void);

int link_start_server(const char *port);

int link_connect_to_server(const char *address, const char *port);

int link_init_transfer(int sfd, gb_t *gb, gb_t **linked_gb);

/**
 * @return 0 if connection is lost, else 1
 */
int link_exchange_joypad(int sfd, gb_t *gb, gb_t *linked_gb);
