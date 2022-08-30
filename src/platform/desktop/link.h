#pragma once

int link_start_server(const char *port, int is_ipv6, int mptcp_enabled);

int link_connect_to_server(const char *address, const char *port, int is_ipv6, int mptcp_enabled);

int link_init_transfer(emulator_t *emu, emulator_t **linked_emu);

void link_send_joypad(byte_t joypad);

int link_poll_joypad(emulator_t *emu);
